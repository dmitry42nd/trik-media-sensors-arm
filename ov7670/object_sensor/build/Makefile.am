MAIN_TARGET_NAME        = object_sensors_arm
AM_CPPFLAGS             = -I ../include -Wall -Wextra
AM_CXXFLAGS             = -Weffc++
VPATH                   += ../src

bin_PROGRAMS            = $(MAIN_TARGET_NAME)
nodist_object_sensors_arm_SOURCES       = $(srcdir)/config.h

object_sensors_arm_SOURCES      = $(srcdir)/../src/main.c \
                          $(srcdir)/../src/module_ce.c \
                          $(srcdir)/../src/module_fb.c \
                          $(srcdir)/../src/module_rc.c \
                          $(srcdir)/../src/module_v4l2.c \
                          $(srcdir)/../src/runtime.c \
                          $(srcdir)/../src/thread_input.c \
                          $(srcdir)/../src/thread_video.c

