MAIN_TARGET_NAME        = jpeg_encoder_arm
DSP_HEADERS_DIR         = $(srcdir)/../../../../dsp/ov7670/jpeg_encoder/

AM_CPPFLAGS		= -I$(DSP_HEADERS_DIR) -I../include -Wall -Wextra
AM_CXXFLAGS		= -Weffc++

bin_PROGRAMS		= $(MAIN_TARGET_NAME)

jpeg_encoder_arm_SOURCES	= $(srcdir)/../src/main.c \
				  $(srcdir)/../src/module_ce.c \
				  $(srcdir)/../src/module_fb.c \
				  $(srcdir)/../src/module_rc.c \
				  $(srcdir)/../src/module_v4l2.c \
				  $(srcdir)/../src/runtime.c \
				  $(srcdir)/../src/thread_input.c \
				  $(srcdir)/../src/thread_video.c


#TESTS			= test-xxx
#noinst_PROGRAMS	= $(TESTS)
#SOURCES		= $(top_srcdir)/tests/test.h
#test_xxx_SOURCES	= $(SOURCES) $(top_srcdir)/tests/test_xxx.c

