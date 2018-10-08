#ifdef CS333_P2
#include "types.h"
#include "user.h"

static void padmilliseconds(int);

void
padmilliseconds(int milliseconds)
{
  if(milliseconds == 0)
    printf(1, "000");
  if(milliseconds < 10 && milliseconds > 0)
    printf(1, "00");
  if(milliseconds < 100 && milliseconds >= 10)
    printf(1, "0");
}

int
main(int argc, char* argv[])
{
  int start;
  int pid;
  int runtime;
  int ms;

  start = uptime();
  pid = fork();

  if(pid < 0) {
    printf(2, "Error relating to fork() call\n");
    exit();
  }
  else if (pid == 0) {
    exec(argv[1], &argv[1]);
    exit();
  }
  else {
    wait();
    printf(1, "%s ran in ", argv[1]);
    runtime = uptime() - start;
    ms = runtime % 1000;
    runtime = runtime/1000;
    printf(1, "%d.", runtime);
    padmilliseconds(ms);
    printf(1, "%d seconds.\n", ms);
  }
  exit();
}
#endif // CS333_P2
