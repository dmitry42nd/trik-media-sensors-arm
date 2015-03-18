AM_CPPFLAGS		= -I$(top_srcdir)/include -Wall -Wextra
AM_CXXFLAGS		= -Weffc++

bin_PROGRAMS		= rover-cv
nodist_rover_cv_SOURCES	= $(top_srcdir)/config.h

rover_cv_SOURCES	= $(top_srcdir)/src/main.c \
			  $(top_srcdir)/src/module_ce.c \
			  $(top_srcdir)/src/module_fb.c \
			  $(top_srcdir)/src/module_rc.c \
			  $(top_srcdir)/src/module_v4l2.c \
			  $(top_srcdir)/src/runtime.c \
			  $(top_srcdir)/src/thread_input.c \
			  $(top_srcdir)/src/thread_video.c


#TESTS			= test-xxx
#noinst_PROGRAMS		= $(TESTS)
#SOURCES			= $(top_srcdir)/tests/test.h
#test_xxx_SOURCES	= $(SOURCES) $(top_srcdir)/tests/test_xxx.c

