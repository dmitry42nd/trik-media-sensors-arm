#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/select.h>
#include <unistd.h>

#include "internal/thread_video.h"
#include "internal/runtime.h"
#include "internal/module_ce.h"
#include "internal/module_fb.h"
#include "internal/module_v4l2.h"


static int threadVideoSelectLoop(Runtime* _runtime, CodecEngine* _ce, V4L2Input* _v4l2, FBOutput* _fb)
{
  int res;
  int maxFd = 0;
  fd_set fdsIn;
  static const struct timespec s_selectTimeout = { .tv_sec=1, .tv_nsec=0 };

  if (_runtime == NULL || _ce == NULL || _v4l2 == NULL || _fb == NULL)
    return EINVAL;

  FD_ZERO(&fdsIn);

  FD_SET(_v4l2->m_fd, &fdsIn);
  if (maxFd < _v4l2->m_fd)
    maxFd = _v4l2->m_fd;

  if ((res = pselect(maxFd+1, &fdsIn, NULL, NULL, &s_selectTimeout, NULL)) < 0)
  {
    res = errno;
    fprintf(stderr, "pselect() failed: %d\n", res);
    return res;
  }

  if (!FD_ISSET(_v4l2->m_fd, &fdsIn))
  {
    fprintf(stderr, "pselect() did not select V4L2\n");
    return EBUSY;
  }

  const void* frameSrcPtr;
  size_t frameSrcSize;
  size_t frameSrcIndex;
  if ((res = v4l2InputGetFrame(_v4l2, &frameSrcPtr, &frameSrcSize, &frameSrcIndex)) != 0)
  {
    fprintf(stderr, "v4l2InputGetFrame() failed: %d\n", res);
    return res;
  }

  void* frameDstPtr;
  size_t frameDstSize;


  if ((res = fbOutputGetFrame(_fb, &frameDstPtr, &frameDstSize)) != 0)
  {
    fprintf(stderr, "fbOutputGetFrame() failed: %d\n", res);
    return res;
  }


  TargetDetectParams  targetDetectParams;
  TargetDetectCommand targetDetectCommand;
  TargetLocation      targetLocation;
  TargetDetectParams  targetDetectParamsResult;
  if ((res = runtimeGetTargetDetectParams(_runtime, &targetDetectParams)) != 0)
  {
    fprintf(stderr, "runtimeGetTargetDetectParams() failed: %d\n", res);
    return res;
  }
  if ((res = runtimeFetchTargetDetectCommand(_runtime, &targetDetectCommand)) != 0)
  {
    fprintf(stderr, "runtimeGetTargetDetectCommand() failed: %d\n", res);
    return res;
  }

  if ((res = runtimeGetVideoOutParams(_runtime, &(_ce->m_videoOutEnable))) != 0)
  {
    fprintf(stderr, "runtimeGetVideoOutParams() failed: %d\n", res);
    return res;
  }


  size_t frameDstUsed = frameDstSize;
  if ((res = codecEngineTranscodeFrame(_ce,
                                       frameSrcPtr, frameSrcSize,
                                       frameDstPtr, frameDstSize, &frameDstUsed,
                                       &targetDetectParams,
                                       &targetDetectCommand,
                                       &targetLocation,
                                       &targetDetectParamsResult)) != 0)
  {
    fprintf(stderr, "codecEngineTranscodeFrame(%p[%zu] -> %p[%zu]) failed: %d\n",
            frameSrcPtr, frameSrcSize, frameDstPtr, frameDstSize, res);
    return res;
  }


  if ((res = fbOutputPutFrame(_fb)) != 0)
  {
    fprintf(stderr, "fbOutputPutFrame() failed: %d\n", res);
    return res;
  }

  if ((res = v4l2InputPutFrame(_v4l2, frameSrcIndex)) != 0)
  {
    fprintf(stderr, "v4l2InputPutFrame() failed: %d\n", res);
    return res;
  }


  switch (targetDetectCommand.m_cmd)
  {
    case 1:
      if ((res = runtimeReportTargetDetectParams(_runtime, &targetDetectParamsResult)) != 0)
      {
        fprintf(stderr, "runtimeReportTargetDetectParams() failed: %d\n", res);
        return res;
      }
      break;

    case 0:
    default:
      if ((res = runtimeReportTargetLocation(_runtime, &targetLocation)) != 0)
      {
        fprintf(stderr, "runtimeReportTargetLocation() failed: %d\n", res);
        return res;
      }
      break;
  }

  return 0;
}


void* threadVideo(void* _arg)
{
  int res = 0;
  intptr_t exit_code = 0;
  Runtime* runtime = (Runtime*)_arg;
  CodecEngine* ce;
  V4L2Input* v4l2;
  FBOutput* fb;
  struct timespec last_fps_report_time;

  if (runtime == NULL)
  {
    exit_code = EINVAL;
    goto exit;
  }

  if (   (ce   = runtimeModCodecEngine(runtime)) == NULL
      || (v4l2 = runtimeModV4L2Input(runtime))   == NULL
      || (fb   = runtimeModFBOutput(runtime))    == NULL)
  {
    exit_code = EINVAL;
    goto exit;
  }


  if ((res = codecEngineAdd(ce, runtimeCfgCodecEngine(runtime))) != 0)
  {
    fprintf(stderr, "codecEngineAdd() failed: %d\n", res);
    exit_code = res;
    goto exit;
  }

  reopen_threadVideo:
  if ((res = codecEngineOpen(ce, runtimeCfgCodecEngine(runtime))) != 0)
  {
    fprintf(stderr, "codecEngineOpen() failed: %d\n", res);
    exit_code = res;
    goto exit;
  }


  if ((res = v4l2InputOpen(v4l2, runtimeCfgV4L2Input(runtime))) != 0)
  {
    fprintf(stderr, "v4l2InputOpen() failed: %d\n", res);
    exit_code = res;
    goto exit_ce_close;
  }

  if ((res = fbOutputOpen(fb, runtimeCfgFBOutput(runtime))) != 0)
  {
    fprintf(stderr, "fbOutputOpen() failed: %d\n", res);
    exit_code = res;
    goto exit_v4l2_close;
  }


  ImageDescription srcImageDesc;
  ImageDescription dstImageDesc;
  if ((res = v4l2InputGetFormat(v4l2, &srcImageDesc)) != 0)
  {
    fprintf(stderr, "v4l2InputGetFormat() failed: %d\n", res);
    exit_code = res;
    goto exit_fb_close;
  }
  if ((res = fbOutputGetFormat(fb, &dstImageDesc)) != 0)
  {
    fprintf(stderr, "fbOutputGetFormat() failed: %d\n", res);
    exit_code = res;
    goto exit_fb_close;
  }
  if ((res = codecEngineStart(ce, runtimeCfgCodecEngine(runtime), &srcImageDesc, &dstImageDesc)) != 0)
  {
    fprintf(stderr, "codecEngineStart() failed: %d\n", res);
    exit_code = res;
    goto exit_fb_close;
  }

  if ((res = v4l2InputStart(v4l2)) != 0)
  {
    fprintf(stderr, "v4l2InputStart() failed: %d\n", res);
    exit_code = res;
    goto exit_ce_stop;
  }


  if ((res = fbOutputStart(fb)) != 0)
  {
    fprintf(stderr, "fbOutputStart() failed: %d\n", res);
    exit_code = res;
    goto exit_v4l2_stop;
  }


  if ((res = clock_gettime(CLOCK_MONOTONIC, &last_fps_report_time)) != 0)
  {
    fprintf(stderr, "clock_gettime(CLOCK_MONOTONIC) failed: %d\n", errno);
    exit_code = res;
    goto exit_fb_stop;
  }

  //reset reopening stuff
  runtime->m_state.m_reopenVideoFlag = false;
  runtime->m_state.m_reopenVideoCnt = 0;
  
  printf("Entering video thread loop\n");
  while (!runtimeGetTerminate(runtime))
  {
    struct timespec now;
    long long last_fps_report_elapsed_ms;

    if ((res = clock_gettime(CLOCK_MONOTONIC, &now)) != 0)
    {
      fprintf(stderr, "clock_gettime(CLOCK_MONOTONIC) failed: %d\n", errno);
      exit_code = res;
      goto exit_fb_stop;
    }

    last_fps_report_elapsed_ms = (now.tv_sec  - last_fps_report_time.tv_sec )*1000
                               + (now.tv_nsec - last_fps_report_time.tv_nsec)/1000000;
    if (last_fps_report_elapsed_ms >= 10*1000)
    {
      last_fps_report_time.tv_sec += 10;

      if ((res = codecEngineReportLoad(ce, last_fps_report_elapsed_ms)) != 0)
        fprintf(stderr, "codecEngineReportLoad() failed: %d\n", res);

      if ((res = v4l2InputReportFPS(v4l2, last_fps_report_elapsed_ms)) != 0)
        fprintf(stderr, "v4l2InputReportFPS() failed: %d\n", res);
    }


    if ((res = threadVideoSelectLoop(runtime, ce, v4l2, fb)) != 0)
    {
      fprintf(stderr, "threadVideoSelectLoop() failed: %d\n", res);
      exit_code = res;
      runtime->m_state.m_reopenVideoCnt = 0;
      runtime->m_state.m_reopenVideoFlag  = true;
      goto exit_fb_stop;
    }
  }
  printf("Left video thread loop\n");


 exit_fb_stop:
  if ((res = fbOutputStop(fb)) != 0)
    fprintf(stderr, "fbOutputStop() failed: %d\n", res);

 exit_v4l2_stop:
  if ((res = v4l2InputStop(v4l2)) != 0)
    fprintf(stderr, "v4l2InputStop() failed: %d\n", res);

 exit_ce_stop:
  if ((res = codecEngineStop(ce)) != 0)
    fprintf(stderr, "codecEngineStop() failed: %d\n", res);


 exit_fb_close:
  if ((res = fbOutputClose(fb)) != 0)
    fprintf(stderr, "fbOutputClose() failed: %d\n", res);

 exit_v4l2_close:
  if ((res = v4l2InputClose(v4l2)) != 0)
    fprintf(stderr, "v4l2InputClose() failed: %d\n", res);

 exit_ce_close:
  if ((res = codecEngineClose(ce)) != 0)
    fprintf(stderr, "codecEngineClose() failed: %d\n", res);

 exit:
   if(runtime->m_state.m_reopenVideoFlag && 
      runtime->m_state.m_reopenVideoCnt < runtime->m_config.m_reopenVideoTries) {
    runtime->m_state.m_reopenVideoCnt++;
    sleep(1); 
    fprintf(stderr, "Reopen video thread: %d\n", runtime->m_state.m_reopenVideoCnt);
    goto reopen_threadVideo;    
  }
 
  runtimeSetTerminate(runtime);
  return (void*)exit_code;
}




