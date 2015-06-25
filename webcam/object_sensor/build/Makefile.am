MAIN_TARGET_NAME        = object_sensor_arm
DSP_HEADERS_DIR         = $(srcdir)/../../../../dsp/webcam/object_sensor/

AM_CPPFLAGS             = -I$(DSP_HEADERS_DIR) -I../include -Wall -Wextra
AM_CXXFLAGS             = -Weffc++

bin_PROGRAMS            = $(MAIN_TARGET_NAME)

object_sensors_arm_SOURCES      = $(srcdir)/../src/main.c \
                                  $(srcdir)/../src/module_ce.c \
                                  $(srcdir)/../src/module_fb.c \
                                  $(srcdir)/../src/module_rc.c \
                                  $(srcdir)/../src/module_v4l2.c \
                                  $(srcdir)/../src/runtime.c \
                                  $(srcdir)/../src/thread_input.c \
                                  $(srcdir)/../src/thread_video.c

