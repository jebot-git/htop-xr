// SPDX-License-Identifier: GPL-2.0-or-later
#include "Model.h"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <iostream>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
int main() {
   pid_t parent = getpid();
   pid_t child = fork();
   assert(child >= 0);
   if (!child) {
      if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0 || getppid() != parent)
         _exit(127);
      execlp("python3", "python3", "-c", "a=bytearray(16*1024*1024)\nwhile True: pass",
             "Žluťoučký-htop-fixture", nullptr);
      _exit(127);
   }
   struct Cleanup {
      pid_t pid;
      ~Cleanup() {
         kill(pid, SIGKILL);
         waitpid(pid, nullptr, 0);
      }
   } cleanup{child};
   assert(HtopGui_init() == 0);
   hgui::snapshot();
   usleep(500000);
   auto s = hgui::snapshot();
   assert(s["cpus"].get<int>() > 0 && s["tasks"].get<int>() > 0);
   auto rows = hgui::visible(s, "žLUŤoučKÝ-HTOP-FIXTURE", 6, true);
   auto found = std::find_if(rows.begin(), rows.end(), [&](auto &p) { return p["pid"] == child; });
   assert(found != rows.end());
   assert((*found)["resident_kib"].get<long>() >= 16384 && (*found)["cpu"].get<double>() > 0);
   auto sorted = hgui::visible(s, "", 0, false);
   for (size_t i = 1; i < sorted.size(); ++i)
      assert(sorted[i - 1]["pid"].get<int>() < sorted[i]["pid"].get<int>());
   for (auto &m : s["meters"])
      assert(m["value"].get<double>() >= 0 && m["value"].get<double>() <= 1);
   assert(HtopGui_openTarget(1) == -1 && errno == EINVAL);
   int fd = HtopGui_openTarget(child);
   assert(fd >= 0);
   assert(HtopGui_signalTarget(fd, SIGKILL) == EINVAL);
   assert(HtopGui_signalTarget(fd, SIGSTOP) == 0);
   int status = 0;
   assert(waitpid(child, &status, WUNTRACED) == child && WIFSTOPPED(status));
   assert(HtopGui_signalTarget(fd, SIGCONT) == 0);
   assert(HtopGui_signalTarget(fd, SIGTERM) == 0);
   assert(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
   assert(HtopGui_signalTarget(fd, SIGCONT) == ESRCH);
   close(fd);
   HtopGui_done();
   std::cout << "PASS upstream collectors, Unicode filter, sorting, memory/CPU, pinned process actions\n";
}
