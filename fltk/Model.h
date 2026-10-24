// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Collector.h"
#include <algorithm>
#include <cmath>
#include <glib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace hgui {
using Json = nlohmann::json;
inline std::string utf8(const char *text) {
   auto valid = g_utf8_make_valid(text ? text : "", -1);
   std::string result(valid);
   g_free(valid);
   return result;
}
inline double finite(double value) {
   return std::isfinite(value) ? value : 0.;
}
inline Json snapshot() {
   const auto *sample = HtopGui_sample();
   Json rows = Json::array();
   static const char states[] = "!?URQWDBPTtZXIS";
   for (size_t i = 0; i < sample->count; ++i) {
      const auto &p = sample->rows[i];
      rows.push_back(
          {{"pid", p.pid},
           {"parent", p.parent},
           {"user", utf8(p.user)},
           {"uid", p.uid},
           {"priority", p.priority},
           {"nice", p.nice},
           {"threads", p.threads},
           {"state",
            std::string(1, p.state > 0 && p.state < int(sizeof(states) - 1) ? states[p.state] : '?')},
           {"cpu", finite(p.cpu)},
           {"memory", finite(p.memory)},
           {"resident_kib", p.resident},
           {"virtual_kib", p.virtualMemory},
           {"start", p.startTime},
           {"time", p.time},
           {"command", utf8(p.command)}});
   }
   return {{"version", 1},
           {"collector", "upstream htop"},
           {"cpus", sample->cpus},
           {"tasks", sample->tasks},
           {"running", sample->running},
           {"meters", Json::array({{{"label", "CPU"},
                                    {"value", std::clamp(finite(sample->cpu), 0., 1.)},
                                    {"text", utf8(sample->cpuText)}},
                                   {{"label", "RAM"},
                                    {"value", std::clamp(finite(sample->memory), 0., 1.)},
                                    {"text", utf8(sample->memoryText)}},
                                   {{"label", "SWAP"},
                                    {"value", std::clamp(finite(sample->swap), 0., 1.)},
                                    {"text", utf8(sample->swapText)}}})},
           {"load", {finite(sample->load[0]), finite(sample->load[1]), finite(sample->load[2])}},
           {"processes", rows}};
}
inline std::vector<Json> visible(const Json &sample, const std::string &query, int sort, bool reverse) {
   std::vector<Json> result;
   auto needle = g_utf8_casefold(query.c_str(), -1);
   for (const auto &p : sample["processes"]) {
      auto text = std::to_string(p["pid"].get<int>()) + " " + p["user"].get<std::string>() + " " +
                  p["command"].get<std::string>();
      auto folded = g_utf8_casefold(text.c_str(), -1);
      if (std::string(folded).find(needle) != std::string::npos)
         result.push_back(p);
      g_free(folded);
   }
   g_free(needle);
   const char *fields[] = {"pid",   "user", "priority", "nice",   "resident_kib",
                           "state", "cpu",  "memory",   "command"};
   std::stable_sort(result.begin(), result.end(), [&](const Json &a, const Json &b) {
      auto key = fields[std::clamp(sort, 0, 8)];
      if (a[key] == b[key])
         return a["pid"].get<int>() < b["pid"].get<int>();
      return reverse ? b[key] < a[key] : a[key] < b[key];
   });
   return result;
}
} // namespace hgui
