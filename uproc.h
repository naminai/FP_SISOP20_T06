#define STRMAX 32

// The uproc struct contains limited fields from the proc struct to be used with 'ps' command
struct uproc {
  uint pid;               // Process ID
  uint uid;               // Process User ID
  uint gid;               // Process Group ID
  uint ppid;              // Parent process' ID
  uint priority;          // Priority level of each process
  uint elapsed_ticks;     // Time since process started
  uint CPU_total_ticks;   // Total elapsed ticks in CPU
  char state[STRMAX];     // Process state
  uint size;              // Size of process memory (bytes)
  char name[STRMAX];      // Process name
};
