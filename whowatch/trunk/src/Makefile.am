SUBDIRS = sys

bin_PROGRAMS = whowatch

whowatch_SOURCES = block.cc help.cc info_box.cc input_box.cc kbd.cc kbd.h list.h \
                   menu.cc menu_hooks.cc menu_hooks.h owner.cc pluglib.cc \
                   pluglib.h process.cc proctree.cc proctree.h screen.cc \
                   search.cc subwin.cc subwin.h term.cc user.cc user_plugin.cc \
                   whowatch.cc whowatch.h
whowatch_LDADD = sys/$(SYSTEM)/lib$(SYSTEM).a

EXTRA_DIST = test.c

