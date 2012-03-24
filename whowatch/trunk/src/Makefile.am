SUBDIRS = sys

bin_PROGRAMS = whowatch

whowatch_SOURCES = help.c info_box.c input_box.c kbd.c kbd.h list.h \
                   menu.c menu_hooks.c menu_hooks.h owner.c pluglib.c \
                   pluglib.h process.c proctree.c proctree.h screen.c \
                   search.c subwin.c subwin.h user.c user_plugin.c \
                   util.c whowatch.c whowatch.h
whowatch_LDADD = sys/$(SYSTEM)/lib$(SYSTEM).a

EXTRA_DIST = test.c

