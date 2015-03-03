#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>

#include "internal/runtime.h"
#include "internal/thread_input.h"
#include "internal/thread_video.h"




static const RuntimeConfig s_runtimeConfig = {
  .m_verbose = false,
  .m_codecEngineConfig = { "dsp_server.xe674", "vidtranscode_cv" },
  .m_v4l2Config        = { "/dev/video0", 320, 240, V4L2_PIX_FMT_YUYV },
  .m_fbConfig          = { "/dev/fb0" },
  .m_rcConfig          = { "/run/object-sensor.in.fifo", "/run/object-sensor.out.fifo", true }
};




void runtimeReset(Runtime* _runtime)
{
  memset(_runtime, 0, sizeof(*_runtime));

  _runtime->m_config = s_runtimeConfig;

  memset(&_runtime->m_modules.m_codecEngine,  0, sizeof(_runtime->m_modules.m_codecEngine));
  memset(&_runtime->m_modules.m_v4l2Input,    0, sizeof(_runtime->m_modules.m_v4l2Input));
  _runtime->m_modules.m_v4l2Input.m_fd = -1;
  memset(&_runtime->m_modules.m_fbOutput,     0, sizeof(_runtime->m_modules.m_fbOutput));
  _runtime->m_modules.m_fbOutput.m_fd = -1;
  memset(&_runtime->m_modules.m_rcInput,      0, sizeof(_runtime->m_modules.m_rcInput));
  _runtime->m_modules.m_rcInput.m_fifoInputFd  = -1;
  _runtime->m_modules.m_rcInput.m_fifoOutputFd = -1;

  memset(&_runtime->m_threads, 0, sizeof(_runtime->m_threads));
  _runtime->m_threads.m_terminate = true;

  pthread_mutex_init(&_runtime->m_state.m_mutex, NULL);
  memset(&_runtime->m_state.m_targetDetectParams,  0, sizeof(_runtime->m_state.m_targetDetectParams));
  memset(&_runtime->m_state.m_targetDetectCommand, 0, sizeof(_runtime->m_state.m_targetDetectCommand));
}




bool runtimeParseArgs(Runtime* _runtime, int _argc, char* const _argv[])
{
  int opt;
  int longopt;
  RuntimeConfig* cfg;

  static const char* s_optstring = "vh";
  static const struct option s_longopts[] =
  {
    { "ce-server",		1,	NULL,	0   }, // 0
    { "ce-codec",		1,	NULL,	0   },
    { "v4l2-path",		1,	NULL,	0   }, // 2
    { "v4l2-width",		1,	NULL,	0   },
    { "v4l2-height",		1,	NULL,	0   },
    { "v4l2-format",		1,	NULL,	0   },
    { "fb-path",		1,	NULL,	0   }, // 6
    { "rc-fifo-in",		1,	NULL,	0   }, // 7
    { "rc-fifo-out",		1,	NULL,	0   },
    { "video-out",		1,	NULL,	0   },
    { "verbose",		0,	NULL,	'v' },
    { "help",			0,	NULL,	'h' },
    { NULL,			0,	NULL,	0   }
  };

  if (_runtime == NULL)
    return false;

  cfg = &_runtime->m_config;


  while ((opt = getopt_long(_argc, _argv, s_optstring, s_longopts, &longopt)) != -1)
  {
    switch (opt)
    {
      case 'v':	cfg->m_verbose = true;					break;

      case 0:
        switch (longopt)
        {
          case 0: cfg->m_codecEngineConfig.m_serverPath = optarg;	break;
          case 1: cfg->m_codecEngineConfig.m_codecName = optarg;	break;

          case 2: cfg->m_v4l2Config.m_path = optarg;			break;
          case 3: cfg->m_v4l2Config.m_width = atoi(optarg);		break;
          case 4: cfg->m_v4l2Config.m_height = atoi(optarg);		break;
          case 5:
            if      (!strcasecmp(optarg, "rgb888"))	cfg->m_v4l2Config.m_format = V4L2_PIX_FMT_RGB24;
            else if (!strcasecmp(optarg, "rgb565"))	cfg->m_v4l2Config.m_format = V4L2_PIX_FMT_RGB565;
            else if (!strcasecmp(optarg, "rgb565x"))	cfg->m_v4l2Config.m_format = V4L2_PIX_FMT_RGB565X;
            else if (!strcasecmp(optarg, "yuv444"))	cfg->m_v4l2Config.m_format = V4L2_PIX_FMT_YUV32;
            else if (!strcasecmp(optarg, "yuv422"))	cfg->m_v4l2Config.m_format = V4L2_PIX_FMT_YUYV;
            else
            {
              fprintf(stderr, "Unknown v4l2 format '%s'\n"
                              "Known formats: rgb888, rgb565, rgb565x, yuv444, yuv422\n",
                      optarg);
              return false;
            }
            break;

          case 6: cfg->m_fbConfig.m_path = optarg;						break;

          case 7  : cfg->m_rcConfig.m_fifoInput  = optarg;					break;
          case 7+1: cfg->m_rcConfig.m_fifoOutput = optarg;					break;
          case 7+2: cfg->m_rcConfig.m_videoOutEnable = atoi(optarg); break;

          default:
            return false;
        }
        break;

      case 'h':
      default:
        return false;
    }
  }

  return true;
}




void runtimeArgsHelpMessage(Runtime* _runtime, const char* _arg0)
{
  if (_runtime == NULL)
    return;

  fprintf(stderr, "Usage:\n"
                  "    %s <opts>\n"
                  " where opts are:\n"
                  "   --ce-server    <dsp-server-name>\n"
                  "   --ce-codec     <dsp-codec-name>\n"
                  "   --v4l2-path    <input-device-path>\n"
                  "   --v4l2-width   <input-width>\n"
                  "   --v4l2-height  <input-height>\n"
                  "   --v4l2-format  <input-pixel-format>\n"
                  "   --fb-path      <output-device-path>\n"
                  "   --rc-fifo-in            <remote-control-fifo-input>\n"
                  "   --rc-fifo-out           <remote-control-fifo-output>\n"
                  "   --video-out             <enable-video-output>\n"
                  "   --verbose\n"
                  "   --help\n",
          _arg0);
}




int runtimeInit(Runtime* _runtime)
{
  int res = 0;
  int exit_code = 0;
  bool verbose;

  if (_runtime == NULL)
    return EINVAL;

  verbose = runtimeCfgVerbose(_runtime);

  if ((res = codecEngineInit(verbose)) != 0)
  {
    fprintf(stderr, "codecEngineInit() failed: %d\n", res);
    exit_code = res;
  }

  if ((res = v4l2InputInit(verbose)) != 0)
  {
    fprintf(stderr, "v4l2InputInit() failed: %d\n", res);
    exit_code = res;
  }

  if ((res = fbOutputInit(verbose)) != 0)
  {
    fprintf(stderr, "fbOutputInit() failed: %d\n", res);
    exit_code = res;
  }

  if ((res = rcInputInit(verbose)) != 0)
  {
    fprintf(stderr, "rcInputInit() failed: %d\n", res);
    exit_code = res;
  }

  return exit_code;
}




int runtimeFini(Runtime* _runtime)
{
  int res;

  if (_runtime == NULL)
    return EINVAL;

  if ((res = rcInputFini()) != 0)
    fprintf(stderr, "rcInputFini() failed: %d\n", res);

  if ((res = fbOutputFini()) != 0)
    fprintf(stderr, "fbOutputFini() failed: %d\n", res);

  if ((res = v4l2InputFini()) != 0)
    fprintf(stderr, "v4l2InputFini() failed: %d\n", res);

  if ((res = codecEngineFini()) != 0)
    fprintf(stderr, "codecEngineFini() failed: %d\n", res);

  return 0;
}




int runtimeStart(Runtime* _runtime)
{
  int res;
  int exit_code = 0;
  RuntimeThreads* rt;

  if (_runtime == NULL)
    return EINVAL;

  rt = &_runtime->m_threads;
  rt->m_terminate = false;

  if ((res = pthread_create(&rt->m_inputThread, NULL, &threadInput, _runtime)) != 0)
  {
    fprintf(stderr, "pthread_create(input) failed: %d\n", res);
    exit_code = res;
    goto exit;
  }

  if ((res = pthread_create(&rt->m_videoThread, NULL, &threadVideo, _runtime)) != 0)
  {
    fprintf(stderr, "pthread_create(video) failed: %d\n", res);
    exit_code = res;
    goto exit_join_input_thread;
  }

  return 0;


 //exit_join_video_thread:
  runtimeSetTerminate(_runtime);
  pthread_cancel(rt->m_videoThread);
  pthread_join(rt->m_videoThread, NULL);

 exit_join_input_thread:
  runtimeSetTerminate(_runtime);
  pthread_cancel(rt->m_inputThread);
  pthread_join(rt->m_inputThread, NULL);

 exit:
  runtimeSetTerminate(_runtime);
  return exit_code;
}




int runtimeStop(Runtime* _runtime)
{
  RuntimeThreads* rt;

  if (_runtime == NULL)
    return EINVAL;

  rt = &_runtime->m_threads;

  runtimeSetTerminate(_runtime);
  pthread_join(rt->m_videoThread, NULL);
  pthread_join(rt->m_inputThread, NULL);

  return 0;
}




bool runtimeCfgVerbose(const Runtime* _runtime)
{
  if (_runtime == NULL)
    return false;

  return _runtime->m_config.m_verbose;
}

const CodecEngineConfig* runtimeCfgCodecEngine(const Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_config.m_codecEngineConfig;
}

const V4L2Config* runtimeCfgV4L2Input(const Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_config.m_v4l2Config;
}

const FBConfig* runtimeCfgFBOutput(const Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_config.m_fbConfig;
}

const RCConfig* runtimeCfgRCInput(const Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_config.m_rcConfig;
}




CodecEngine* runtimeModCodecEngine(Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_modules.m_codecEngine;
}

V4L2Input* runtimeModV4L2Input(Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_modules.m_v4l2Input;
}

FBOutput* runtimeModFBOutput(Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_modules.m_fbOutput;
}

RCInput* runtimeModRCInput(Runtime* _runtime)
{
  if (_runtime == NULL)
    return NULL;

  return &_runtime->m_modules.m_rcInput;
}




bool runtimeGetTerminate(Runtime* _runtime)
{
  if (_runtime == NULL)
    return true;

  return _runtime->m_threads.m_terminate;
}

void runtimeSetTerminate(Runtime* _runtime)
{
  if (_runtime == NULL)
    return;

  _runtime->m_threads.m_terminate = true;
}

int runtimeGetTargetDetectParams(Runtime* _runtime, TargetDetectParams* _targetDetectParams)
{
  if (_runtime == NULL || _targetDetectParams == NULL)
    return EINVAL;

  pthread_mutex_lock(&_runtime->m_state.m_mutex);
  *_targetDetectParams = _runtime->m_state.m_targetDetectParams;
  pthread_mutex_unlock(&_runtime->m_state.m_mutex);
  return 0;
}

int runtimeSetTargetDetectParams(Runtime* _runtime, const TargetDetectParams* _targetDetectParams)
{
  if (_runtime == NULL || _targetDetectParams == NULL)
    return EINVAL;

  pthread_mutex_lock(&_runtime->m_state.m_mutex);
  _runtime->m_state.m_targetDetectParams = *_targetDetectParams;
  pthread_mutex_unlock(&_runtime->m_state.m_mutex);
  return 0;
}

int runtimeGetVideoOutParams(Runtime* _runtime, bool* _videoOutEnable)
{
  if (_runtime == NULL || _videoOutEnable == NULL)
    return EINVAL;

  pthread_mutex_lock(&_runtime->m_state.m_mutex);
  *_videoOutEnable = _runtime->m_state.m_videoOutEnable;
  pthread_mutex_unlock(&_runtime->m_state.m_mutex);
  return 0;
}

int runtimeSetVideoOutParams(Runtime* _runtime, const bool* _videoOutEnable)
{
  if (_runtime == NULL || _videoOutEnable == NULL)
    return EINVAL;

  pthread_mutex_lock(&_runtime->m_state.m_mutex);
  _runtime->m_state.m_videoOutEnable = *_videoOutEnable;
  pthread_mutex_unlock(&_runtime->m_state.m_mutex);
  return 0;
}

int runtimeFetchTargetDetectCommand(Runtime* _runtime, TargetDetectCommand* _targetDetectCommand)
{
  if (_runtime == NULL || _targetDetectCommand == NULL)
    return EINVAL;

  pthread_mutex_lock(&_runtime->m_state.m_mutex);
  *_targetDetectCommand = _runtime->m_state.m_targetDetectCommand;
  _runtime->m_state.m_targetDetectCommand.m_cmd = 0;
  pthread_mutex_unlock(&_runtime->m_state.m_mutex);
  return 0;
}

int runtimeSetTargetDetectCommand(Runtime* _runtime, const TargetDetectCommand* _targetDetectCommand)
{
  if (_runtime == NULL || _targetDetectCommand == NULL)
    return EINVAL;

  pthread_mutex_lock(&_runtime->m_state.m_mutex);
  _runtime->m_state.m_targetDetectCommand = *_targetDetectCommand;
  pthread_mutex_unlock(&_runtime->m_state.m_mutex);
  return 0;
}

int runtimeReportTargetLocation(Runtime* _runtime, const TargetLocation* _targetLocation)
{
  if (_runtime == NULL || _targetLocation == NULL)
    return EINVAL;

#warning Unsafe
  rcInputUnsafeReportTargetLocation(&_runtime->m_modules.m_rcInput, _targetLocation);

  return 0;
}

int runtimeReportTargetDetectParams(Runtime* _runtime, const TargetDetectParams* _targetDetectParams)
{
  if (_runtime == NULL || _targetDetectParams == NULL)
    return EINVAL;

#warning Unsafe
  rcInputUnsafeReportTargetDetectParams(&_runtime->m_modules.m_rcInput, _targetDetectParams);

  return 0;
}


