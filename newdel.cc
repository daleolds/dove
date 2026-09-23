#include <stdlib.h>

/*--------------------------------------------------------------------------
 * array new and delete on top of malloc and free, so the binary does not
 * need libstdc++. new[] returns null on failure rather than throwing, which
 * the existing null checks (e.g. in kill.cc) rely on.
 */
void *operator new[](size_t n) { return malloc(n); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
