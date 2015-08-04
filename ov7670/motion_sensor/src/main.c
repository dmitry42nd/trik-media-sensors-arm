#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <errno.h>
#include <signal.h>

#include "internal/runtime.h"




static sig_atomic_t s_signalTerminate = false;
static void sigterm_action(int _signal, siginfo_t* _siginfo, void* _context)
{
  (void)_signal;
  (void)_siginfo;
  (void)_context;
  s_signalTerminate = true;
}
static void sigactions_setup()
{
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = &sigterm_action;
  action.sa_flags = SA_SIGINFO|SA_RESTART;

  if (sigaction(SIGTERM, &action, NULL) != 0)
    fprintf(stderr, "sigaction(SIGTERM) failed: %d\n", errno);
  if (sigaction(SIGINT,  &action, NULL) != 0)
    fprintf(stderr, "sigaction(SIGINT) failed: %d\n", errno);

  signal(SIGPIPE, SIG_IGN);
}




int main(int _argc, char* const _argv[])
{
  int res = 0;
  int exit_code = EX_OK;
  Runtime runtime;
  const char* arg0 = _argv[0];

  sigactions_setup();

  runtimeReset(&runtime);
  if (!runtimeParseArgs(&runtime, _argc, _argv))
  {
    runtimeArgsHelpMessage(&runtime, arg0);
    exit_code = EX_USAGE;
    goto exit;
  }

  if ((res = runtimeInit(&runtime)) != 0)
  {
    fprintf(stderr, "runtimeInit() failed: %d\n", res);
    exit_code = EX_SOFTWARE;
    goto exit;
  }

  if ((res = runtimeStart(&runtime)) != 0)
  {
    fprintf(stderr, "runtimeStart() failed: %d\n", res);
    exit_code = EX_SOFTWARE;
    goto exit_fini;
  }

  printf("Running\n");
  while (!s_signalTerminate && !runtimeGetTerminate(&runtime))
    sleep(1);
  printf("Terminating\n");


 //exit_stop:
  if ((res = runtimeStop(&runtime)) != 0)
    fprintf(stderr, "runtimeStop() failed: %d\n", res);

 exit_fini:
  if ((res = runtimeFini(&runtime)) != 0)
    fprintf(stderr, "runtimeStop() failed: %d\n", res);

 exit:
  return exit_code;
}


