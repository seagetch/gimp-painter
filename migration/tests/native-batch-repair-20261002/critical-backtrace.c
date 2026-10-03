#define _GNU_SOURCE
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
static void critical_trap (int signal_number)
{
  void *frames[64];
  const char marker[] = "DIAGNOSTIC_FATAL_CRITICAL_STACK\n";
  write (STDERR_FILENO, marker, sizeof marker - 1);
  backtrace_symbols_fd (frames, backtrace (frames, 64), STDERR_FILENO);
  _exit (128 + signal_number);
}
__attribute__((constructor)) static void install_critical_trap (void)
{
  struct sigaction action = {0};
  action.sa_handler = critical_trap;
  sigemptyset (&action.sa_mask);
  sigaction (SIGTRAP, &action, 0);
}
