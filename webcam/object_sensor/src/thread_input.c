#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/select.h>

#include "internal/thread_input.h"
#include "internal/runtime.h"
#include "internal/module_rc.h"




static int threadInputSelectLoop(Runtime* _runtime, RCInput* _rc)
{
  int res;
  int maxFd = 0;
  fd_set fdsIn;
  static const struct timespec s_selectTimeout = { .tv_sec=1, .tv_nsec=0 };

  if (_runtime == NULL || _rc == NULL)
    return EINVAL;

  FD_ZERO(&fdsIn);

  if (_rc->m_fifoInputFd != -1)
  {
    FD_SET(_rc->m_fifoInputFd, &fdsIn);
    if (maxFd < _rc->m_fifoInputFd)
      maxFd = _rc->m_fifoInputFd;
  }

  if ((res = pselect(maxFd+1, &fdsIn, NULL, NULL, &s_selectTimeout, NULL)) < 0)
  {
    res = errno;
    fprintf(stderr, "pselect() failed: %d\n", res);
    return res;
  }


  if (_rc->m_fifoInputFd != -1 && FD_ISSET(_rc->m_fifoInputFd, &fdsIn))
  {
    if ((res = rcInputReadFifoInput(_rc)) != 0)
    {
      fprintf(stderr, "rcInputReadFifoInput() failed: %d\n", res);
      return res;
    }
  }

  TargetDetectParams targetDetectParams;
  if ((res = rcInputGetTargetDetectParams(_rc, &targetDetectParams)) != 0)
  {
    if (res != ENODATA)
    {
      fprintf(stderr, "rcInputGetTargetDetectParams() failed: %d\n", res);
      return res;
    }
  }
  else
  {
    if ((res = runtimeSetTargetDetectParams(_runtime, &targetDetectParams)) != 0)
    {
      fprintf(stderr, "runtimeSetTargetDetectParams() failed: %d\n", res);
      return res;
    }
  }

  bool videoOutEnable;
  if ((res = rcInputGetVideoOutParams(_rc, &videoOutEnable)) != 0)
  {
    if (res != ENODATA)
    {
      fprintf(stderr, "rcInputGetVideoOutParams() failed: %d\n", res);
      return res;
    }
  }
  else
  {
    if ((res = runtimeSetVideoOutParams(_runtime, &videoOutEnable)) != 0)
    {
      fprintf(stderr, "runtimeSetVideoOutParams() failed: %d\n", res);
      return res;
    }
  }


  TargetDetectCommand targetDetectCommand;
  if ((res = rcInputGetTargetDetectCommand(_rc, &targetDetectCommand)) != 0)
  {
    if (res != ENODATA)
    {
      fprintf(stderr, "rcInputGetTargetDetectCommand() failed: %d\n", res);
      return res;
    }
  }
  else
  {
    if ((res = runtimeSetTargetDetectCommand(_runtime, &targetDetectCommand)) != 0)
    {
      fprintf(stderr, "runtimeSetTargetDetectCommand() failed: %d\n", res);
      return res;
    }
  }

  return 0;
}




void* threadInput(void* _arg)
{
  int res = 0;
  intptr_t exit_code = 0;
  Runtime* runtime = (Runtime*)_arg;
  RCInput* rc;

  if (runtime == NULL)
  {
    exit_code = EINVAL;
    goto exit;
  }

  if ((rc = runtimeModRCInput(runtime)) == NULL)
  {
    exit_code = EINVAL;
    goto exit;
  }


  if ((res = rcInputOpen(rc, runtimeCfgRCInput(runtime))) != 0)
  {
    fprintf(stderr, "rcInputOpen() failed: %d\n", res);
    exit_code = res;
    goto exit;
  }


  if ((res = rcInputStart(rc)) != 0)
  {
    fprintf(stderr, "rcInputStart() failed: %d\n", res);
    exit_code = res;
    goto exit_rc_close;
  }


  printf("Entering input thread loop\n");
  while (!runtimeGetTerminate(runtime))
  {
    if ((res = threadInputSelectLoop(runtime, rc)) != 0)
    {
      fprintf(stderr, "threadInputSelectLoop() failed: %d\n", res);
      exit_code = res;
      goto exit_rc_stop;
    }
  }
  printf("Left input thread loop\n");


 exit_rc_stop:
  if ((res = rcInputStop(rc)) != 0)
    fprintf(stderr, "rcInputStop() failed: %d\n", res);


 exit_rc_close:
  if ((res = rcInputClose(rc)) != 0)
    fprintf(stderr, "rcInputClose() failed: %d\n", res);


 exit:
  runtimeSetTerminate(runtime);
  return (void*)exit_code;
}




