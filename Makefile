ODIR=obj
ifeq ($(ODIR),$(notdir $(CURDIR)))
#----------------------------------------------------------------------------

VERSION = "2.2.24"
#DEBUG = "debug"

ifeq ($(OS),Windows_NT)
TARGET = dove.exe
PLAT = WIN32
MOD = w32
LDFLAGS =
else
TARGET = dove
PLAT = GNU
MOD = gnu
#LDFLAGS = -lncurses
LDFLAGS = -ltinfo
endif

ifdef DEBUG
OPT = -g
STRIP = 
else
OPT = -Os
STRIP = @strip $(TARGET)
endif

CPPFLAGS = $(OPT) -fno-exceptions -fno-rtti -Wall -Werror -Wno-parentheses \
	-Wno-sign-compare -I$(SRCDIR) -DDOVE_FOR_$(PLAT) -D__STDC_LIMIT_MACROS \
	-DVERSION=\"$(VERSION)$(DEBUG)\"

SRCS = buffer.cc cfgbind.cc cfgcolor.cc cfgmain.cc cfgmode.cc cfgsampl.cc \
	cfgtable.cc cursor.cc display.cc duimenu.cc duiwin.cc file.cc keybrd.cc \
	kill.cc line.cc main.cc mark.cc menu.cc misc.cc msgline.cc pfile$(MOD).cc \
	pkeyb$(MOD).cc popup.cc pvid$(MOD).cc search.cc type.cc view.cc

vpath %.cc $(SRCDIR)
vpath %.h $(SRCDIR)
.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRCS:%.cc=%.o)
	@echo linking $@; g++ -o $@ $^ $(LDFLAGS)
	$(STRIP)

clean:
	@rm -f $(SRCS:%.cc=%.o) $(SRCS:%.cc=%.d) $(TARGET)

install:
	@rm -f /bin/$(TARGET)
	@cp $(TARGET) /usr/bin

%.o: %.cc
	@echo compiling $<; g++ $(CPPFLAGS) -MD -MP -c -o $@ $<

-include $(SRCS:%.cc=%.d)

#----------------------------------------------------------------------------
else

.SUFFIXES:
.PHONY: $(ODIR) clean
$(ODIR):
	+@mkdir -p $@; $(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile SRCDIR=$(CURDIR) $(MAKECMDGOALS)

Makefile : ;
% :: $(ODIR) ; :
clean:
	@rm -rf $(ODIR)

endif
#===========================================================================
