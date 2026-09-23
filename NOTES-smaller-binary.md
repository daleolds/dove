# Notes: compiling dove to a smaller binary

Done on the `smaller-binary` branch (commit 0a52b3b), measured on 2026-09-22 with
g++ 13.3 (Ubuntu 24.04), x86-64. The terminfo dependency had already been removed
in commit 21bf718, so these numbers start from a tree without `libtinfo`.

The stripped release binary went from about **117 KB** to **68 KB**, and libc is
now its only runtime dependency (`libstdc++`, `libgcc_s` and `libm` are gone).

| Change | Stripped size (bytes) |
|---|---|
| Previous flags (`-Os -fno-exceptions -fno-rtti`, plain `strip`) | 117,360 |
| Flags below, without `-DNDEBUG` or the libstdc++ shim | ~73,200 |
| plus the libstdc++ shim | ~72,700 |
| plus `-DNDEBUG` (what the Makefile now builds) | **68,352** |

The middle rows were measured with a one-shot `g++` command in a scratch copy, so
they are approximate.

## What each change does

- **`-no-pie`** is the biggest single item. The pointer tables (`CapMap`,
  `functionTable`) need a relocation each under PIE. Without PIE the code is
  loaded at a fixed address.
- **Unwind tables** are dead weight with `-fno-exceptions`, so
  `-fno-unwind-tables` and `-fno-asynchronous-unwind-tables` cost nothing.
- **`-flto`** is passed at both compile and link time. The link rule now passes
  `$(OPT)` and `$(LDFLAGS)`; before, it was a bare `g++ -o $@ $^`.
- **`-DNDEBUG`** saves about 4.3 KB. Each of the ~46 `assert`s embeds its
  expression and full function signature as strings. It made `wlen` in
  `pvidgnu.cc` `TTFlush` unused, so that variable now has a `(void)wlen;`.
- **libstdc++ shim.** `newdel.cc` maps `operator new[]`/`delete[]` to
  `malloc`/`free`, and the link uses `-nostdlib++ -static-libgcc`.
  `-fno-threadsafe-statics` removes the static-init guards; the code is
  single-threaded. `new[]` returns `malloc`'s result directly, so the null check
  in `kill.cc` (`!(kills = new Bob[MAXKILLS])`) still works.
- **`-Wl,--as-needed`** drops the unused `libm`. It does not change the size.
- `-ffunction-sections`/`--gc-sections` saved only about 40 bytes on top of LTO.
  `--gc-sections` is in the Makefile; the compile-side flags were left out.

## Makefile as built

```make
ifdef DEBUG
OPT = -g
LDFLAGS =
STRIP =
else
OPT = -Os -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables \
	-fno-stack-protector -fcf-protection=none -fno-plt -flto
LDFLAGS = -no-pie -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro \
	-Wl,-z,noseparate-code -Wl,--no-eh-frame-hdr
STRIP = @strip -s -R .comment $(TARGET)
endif

LDFLAGS += -nostdlib++ -static-libgcc -Wl,--as-needed
```

`CPPFLAGS` also gained `-fno-threadsafe-statics`, `SRCS` gained `newdel.cc`, and
the link line is `g++ $(OPT) -o $@ $^ $(LDFLAGS)`. `DEBUG=debug` builds use `-g`
instead of the release flags, but also link without libstdc++.

## Trade-offs

The release build gives up some hardening for size: ASLR for code (`-no-pie`),
RELRO, the stack protector and CET (`-fcf-protection`). It also drops assert
checks. dove opens arbitrary files, so this is a deliberate choice rather than a
free win.

## Tried and rejected

- **`-O2`** is larger than `-Os`, and **`-Oz`** gave the same size as `-Os` on
  GCC 13.
- **Static linking.** `-static-libstdc++` grew the binary to 204 KB.
- **`-Wl,-N`** (merged RWX segment) fails to link.
- **Turning off function alignment and jump tables** saved only 32 bytes.
- **`-D_FORTIFY_SOURCE=0`** would save about 1.1 KB. It was left on because it
  is more hardening for little gain.
- **Repacking `CapMap` (2 KB) and `functionTable` (3.7 KB)** into index or
  `short` fields might save 1-2 KB now that `-no-pie` removed their relocations.
  Not worth the code churn.
- **UPX** isn't installed here. It would likely halve the file, but it adds
  startup cost and sometimes triggers antivirus false positives.

## Testing still needed

Both release and debug builds compile and link. The release binary has only been
started without a tty, where it prints "Failure to initialize menu system." as
expected. Before merging, try it in a real terminal: open, edit and save a file,
resize the window, cut and paste (exercises the kill ring and `new[]`), shell out,
and compare the function and Home/End keys against a build from `main`.
