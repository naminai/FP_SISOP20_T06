#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

#define HEADER "\nPID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\n"

static void padmilliseconds(int);
static void ps();

void
padmilliseconds(int milliseconds)
{
  if(milliseconds < 10 && milliseconds > 0)
    printf(1, "00");
  else if(milliseconds < 100 && milliseconds >= 10)
    printf(1, "0");
  else
    return;
}

void
ps()
{
  uint max = 32;
  struct uproc* table = malloc(sizeof(struct uproc) * max);
  int count = getprocs(max, table);
  int elapsed;
  int milliseconds;
  int cpu;
  int cpu_milliseconds;

  if(count < 0) {
    printf(2, "\nFailure: an error occurred while creating the user process table.\n");
  } else {
    printf(1, HEADER);

    for(int i = 0; i < count; ++i) {
      // Calculate process time
      elapsed = table[i].elapsed_ticks;
      milliseconds = elapsed % 1000;
      elapsed = elapsed/1000;
      // Calculate CPU time
      cpu = table[i].CPU_total_ticks;
      cpu_milliseconds = cpu % 1000;
      cpu = cpu/1000;

      // Print pid, name
      printf(1, "%d\t%s\t", table[i].pid, table[i].name);
      // Print uid, gid, ppid, elapsed (seconds)
      printf(1, "%d\t%d\t%d\t%d.", table[i].uid, table[i].gid, table[i].ppid, elapsed);
      padmilliseconds(milliseconds);
      // Print elapsed (milliseconds), cpu (seconds)
      printf(1, "%d\t%d.", milliseconds, cpu);
      padmilliseconds(cpu_milliseconds);
      // Print cpu (milliseconds), state, size
      printf(1, "%d\t%s\t%d\n", cpu_milliseconds, table[i].state, table[i].size);
    }
  }
  free(table);
}

int
main(int argc, char* argv[])
{
    ps();
  exit();
}
#endif // CS333_P2
