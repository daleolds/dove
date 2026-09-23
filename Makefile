ODIR=obj
ifeq ($(ODIR),$(notdir $(CURDIR)))
#----------------------------------------------------------------------------

VERSION = "2.2.30"
#DEBUG = "debug"

TARGET = dove

ifdef DEBUG
OPT = -g
STRIP =
else
OPT = -Os
STRIP = @strip $(TARGET)
endif

CPPFLAGS = $(OPT) -fno-exceptions -fno-rtti -Wall -Werror -Wno-parentheses \
	-Wno-sign-compare -I$(SRCDIR) -D__STDC_LIMIT_MACROS \
	-DVERSION=\"$(VERSION)$(DEBUG)\"

SRCS = buffer.cc cfgbind.cc cfgcolor.cc cfgmain.cc cfgmode.cc cfgsampl.cc \
	cfgtable.cc cursor.cc display.cc duimenu.cc duiwin.cc file.cc keybrd.cc \
	kill.cc line.cc main.cc mark.cc menu.cc misc.cc msgline.cc pfilegnu.cc \
	pkeybgnu.cc popup.cc pvidgnu.cc search.cc type.cc view.cc

vpath %.cc $(SRCDIR)
vpath %.h $(SRCDIR)
.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRCS:%.cc=%.o)
	@echo linking $@; g++ -o $@ $^
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
