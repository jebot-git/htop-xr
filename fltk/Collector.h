#ifndef HEADER_HtopGuiCollector
#define HEADER_HtopGuiCollector
/* GPL-2.0-or-later: FLTK adapter over the upstream htop collectors. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   int pid, parent, uid, priority, nice, threads, state;
   double cpu, memory;
   long resident, virtualMemory;
   uint64_t startTime, time;
   const char* user;
   const char* command;
} HtopGuiRow;

typedef struct {
   unsigned cpus, tasks, running;
   double cpu, memory, swap, load[3];
   const char* cpuText;
   const char* memoryText;
   const char* swapText;
   const HtopGuiRow* rows;
   size_t count;
} HtopGuiSample;

int HtopGui_init(void);
const HtopGuiSample* HtopGui_sample(void);
int HtopGui_openTarget(int pid);
int HtopGui_signalTarget(int fd, int signal);
void HtopGui_done(void);

#ifdef __cplusplus
}
#endif
#endif
