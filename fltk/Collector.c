/* GPL-2.0-or-later */
#include "config.h" // IWYU pragma: keep

#include "fltk/Collector.h"

#include <errno.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "CPUMeter.h"
#include "CRT.h"
#include "DynamicColumn.h"
#include "DynamicMeter.h"
#include "DynamicScreen.h"
#include "Machine.h"
#include "MemoryMeter.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessTable.h"
#include "Settings.h"
#include "SwapMeter.h"
#include "UsersTable.h"
#include "Vector.h"
#include "XUtils.h"

const char* program = "htop-fltk";
static Machine* host;
static ProcessTable* table;
static Settings* settings;
static UsersTable* users;
static Meter* meters[3];
static HtopGuiSample sample;
static HtopGuiRow* rows;
static int colours[LAST_COLORELEMENT];
static Hashtable* dynamicMeters;
static Hashtable* dynamicColumns;
static Hashtable* dynamicScreens;

int HtopGui_init(void) {
   setlocale(LC_ALL, "");
   /* Command metadata uses colour indices even without a curses screen. */
   CRT_colors = colours;
   if (!Platform_init())
      return -1;
   users = UsersTable_new();
   host = Machine_new(users, (uid_t)-1);
   table = ProcessTable_new(host, NULL);
   /* Never read/migrate/write the console user's htoprc from the frontend. */
   char* original = getenv("HTOPRC") ? xStrdup(getenv("HTOPRC")) : NULL;
   setenv("HTOPRC", "/dev/null", 1);
   dynamicMeters = DynamicMeters_new();
   dynamicColumns = DynamicColumns_new();
   dynamicScreens = DynamicScreens_new();
   settings = Settings_new(host, dynamicMeters, dynamicColumns, dynamicScreens);
   if (original) {
      setenv("HTOPRC", original, 1);
      free(original);
   } else {
      unsetenv("HTOPRC");
   }
   settings->writeConfig = false;
   settings->updateProcessNames = true;
   settings->highlightChanges = false;
   settings->hideKernelThreads = true;
   settings->hideUserlandThreads = true;
   Machine_populateTablesFromSettings(host, settings, &table->super);
   meters[0] = Meter_new(host, 0, &CPUMeter_class);
   meters[1] = Meter_new(host, 0, &MemoryMeter_class);
   meters[2] = Meter_new(host, 0, &SwapMeter_class);
   return 0;
}

const HtopGuiSample* HtopGui_sample(void) {
   Machine_scan(host);
   Machine_scanTables(host);
   for (int i = 0; i < 3; i++)
      Meter_updateValues(meters[i]);
   sample.cpu = Platform_setCPUValues(meters[0], 0) / 100.;
   double used = 0;
   for (unsigned i = 0; i < Platform_numberOfMemoryClasses; i++) {
      if (Platform_memoryClasses[i].countsAsUsed && isfinite(meters[1]->values[i]))
         used += meters[1]->values[i];
   }
   sample.memory = meters[1]->total > 0 ? used / meters[1]->total : 0;
   sample.swap = meters[2]->total > 0 ? meters[2]->values[SWAP_METER_USED] / meters[2]->total : 0;
   sample.cpus = host->activeCPUs;
   sample.tasks = table->totalTasks;
   sample.running = table->runningTasks;
   sample.cpuText = meters[0]->txtBuffer;
   sample.memoryText = meters[1]->txtBuffer;
   sample.swapText = meters[2]->txtBuffer;
   Platform_getLoadAverage(&sample.load[0], &sample.load[1], &sample.load[2]);
   int count = Vector_size(table->super.rows);
   rows = xReallocArray(rows, count ? count : 1, sizeof(*rows));
   sample.count = 0;
   for (int i = 0; i < count; i++) {
      Process* p = (Process*)Vector_get(table->super.rows, i);
      if (!p || !p->super.show || !p->super.updated)
         continue;
      rows[sample.count++] = (HtopGuiRow) {
         .pid = Process_getPid(p), .parent = Process_getParent(p), .uid = p->st_uid,
         .priority = p->priority, .nice = p->nice, .threads = p->nlwp, .state = p->state,
         .cpu = p->percent_cpu, .memory = p->percent_mem,
         .resident = p->m_resident, .virtualMemory = p->m_virt,
         .startTime = p->starttime_ctime, .time = p->time,
         .user = p->user, .command = Process_getCommand(p),
      };
   }
   sample.rows = rows;
   return &sample;
}

int HtopGui_openTarget(int pid) {
   if (pid <= 1) {
      errno = EINVAL;
      return -1;
   }
#ifdef SYS_pidfd_open
   int fd = syscall(SYS_pidfd_open, pid, 0);
   if (fd < 0)
      return -1;
   HtopGui_sample();
   struct pollfd status = {.fd = fd, .events = POLLIN};
   const Process* p = ProcessTable_findProcess(table, pid);
   if (!p || !p->super.updated || poll(&status, 1, 0) != 0) {
      close(fd);
      errno = ESRCH;
      return -1;
   }
   return fd;
#else
   errno = ENOSYS;
   return -1;
#endif
}

int HtopGui_signalTarget(int fd, int sig) {
   if (fd < 0 || (sig != SIGTERM && sig != SIGSTOP && sig != SIGCONT))
      return EINVAL;
#ifdef SYS_pidfd_send_signal
   return syscall(SYS_pidfd_send_signal, fd, sig, NULL, 0) == 0 ? 0 : errno;
#else
   return ENOSYS;
#endif
}

void HtopGui_done(void) {
   if (!host)
      return;
   for (int i = 0; i < 3; i++)
      Meter_delete((Object*)meters[i]);
   Machine_delete(host);
   Settings_delete(settings);
   DynamicColumns_delete(dynamicColumns);
   DynamicMeters_delete(dynamicMeters);
   DynamicScreens_delete(dynamicScreens);
   UsersTable_delete(users);
   Platform_done();
   free(rows);
   host = NULL;
}
