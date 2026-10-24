// SPDX-License-Identifier: GPL-2.0-or-later
#define GL_GLEXT_PROTOTYPES
#include "Model.h"
#include "appearance.h"
#include "desktop-menu.h"
#include "gpu-window.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table_Row.H>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>

using hgui::Json;
static constexpr int FloorW = 1024, FloorH = 256;
static void meters(const Json &sample, int x, int y, int width, int height, bool floor) {
   int row = height / 3;
   const Fl_Color colours[] = {fl_rgb_color(55, 130, 255), fl_rgb_color(45, 235, 170),
                               fl_rgb_color(245, 65, 155)};
   for (int i = 0; i < 3; ++i) {
      const auto &meter = sample["meters"][i];
      int top = y + i * row;
      fl_color(FL_FOREGROUND_COLOR);
      fl_font(dx::font, dx::size);
      auto text = meter["label"].get<std::string>() + "  " + meter["text"].get<std::string>();
      fl_draw(text.c_str(), x + 8, top + 3, width - 16, row / 2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
      int by = top + row / 2 + 2, bh = std::max(6, row / 2 - 8);
      fl_color(FL_BACKGROUND2_COLOR);
      fl_rectf(x + 8, by, width - 16, bh);
      int filled = int((width - 16) * meter["value"].get<double>());
      fl_color(colours[i]);
      // Tiled cells echo cmuxr's floor spectrum, with real measured lengths.
      if (floor)
         for (int px = 0; px < filled; px += 16)
            fl_rectf(x + 8 + px, by, std::min(14, filled - px), bh);
      else
         fl_rectf(x + 8, by, filled, bh);
   }
}
class Monitor;
class Processes : public Fl_Table_Row {
 public:
   Monitor *owner;
   std::vector<Json> data;
   int sort = 6;
   bool reverse = true;
   Processes(int x, int y, int w, int h, Monitor *window);
   void draw_cell(TableContext, int, int, int, int, int, int) override;
};
class Summary : public Fl_Widget {
 public:
   Json sample;
   Summary(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}
   void draw() override {
      if (!sample.empty())
         meters(sample, x(), y(), w(), h(), false);
   }
};
class Monitor : public dx::GpuWindow {
   Fl_Menu_Bar menu{0, 0, 0, 0};
   std::unique_ptr<DesktopMenu> desktop;
   Summary summary{16, 8, 1120, 156};
   Fl_Box info{16, 167, 1120, 32};
   Fl_Input filter{76, 208, 610, 32, "Filter"};
   Fl_Check_Button floor{710, 208, 220, 32, "XR floor readouts"};
   Fl_Button pause{954, 208, 150, 32, "Pause"};
   Processes processes{16, 256, 1120, 480, this};
   Json sample;
   bool paused = false, xrOnly = false;
   int floorLock = -1;
   uint64_t serial = std::chrono::system_clock::now().time_since_epoch().count();
   std::filesystem::path runtime;
   std::chrono::steady_clock::time_point sampled{};
   std::string capture;
   int captureTicks = 0;
   static void tick(void *p) {
      auto self = static_cast<Monitor *>(p);
      try {
         if (!self->paused)
            self->refresh();
      } catch (const std::exception &e) {
         self->info.copy_label(e.what());
      }
      if (!self->capture.empty() && ++self->captureTicks == 2) {
         self->save(self->capture);
         self->hide();
         return;
      }
      Fl::repeat_timeout(1., tick, p);
   }
   static void menuAction(Fl_Widget *w, void *p) {
      auto item = static_cast<Fl_Menu_ *>(w)->mvalue();
      if (item)
         static_cast<Monitor *>(p)->command(item->label());
   }
   void atom(const std::filesystem::path &target, const char *data, size_t size) {
      auto temporary = target.string() + "." + std::to_string(getpid()) + ".tmp";
      {
         std::ofstream out(temporary, std::ios::binary);
         out.write(data, size);
         if (!out)
            throw std::runtime_error("Cannot publish performance floor");
      }
      chmod(temporary.c_str(), 0600);
      std::filesystem::rename(temporary, target);
   }
   std::vector<unsigned char> floorPixels() {
      make_current();
      GLint previous = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
      GLuint fbo = 0, texture = 0;
      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glGenTextures(1, &texture);
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FloorW, FloorH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
         throw std::runtime_error("Floor GPU target unavailable");
      int nativeW = w(), nativeH = h();
      Fl_Widget::resize(x(), y(), FloorW, FloorH);
      draw_begin();
      glViewport(0, 0, FloorW, FloorH);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glOrtho(0, FloorW, FloorH, 0, -1, 1);
      glMatrixMode(GL_MODELVIEW);
      unsigned char r, g, b;
      Fl::get_color(FL_BACKGROUND_COLOR, r, g, b);
      glClearColor(r / 255.f, g / 255.f, b / 255.f, .94f);
      glClear(GL_COLOR_BUFFER_BIT);
      meters(sample, 8, 8, FloorW - 16, FloorH - 16, true);
      draw_end();
      Fl_Widget::resize(x(), y(), nativeW, nativeH);
      std::vector<unsigned char> raw(FloorW * FloorH * 4), result(raw.size());
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, FloorW, FloorH, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
      for (int y = 0; y < FloorH; ++y)
         memcpy(&result[y * FloorW * 4], &raw[(FloorH - 1 - y) * FloorW * 4], FloorW * 4);
      glBindFramebuffer(GL_FRAMEBUFFER, previous);
      glDeleteFramebuffers(1, &fbo);
      glDeleteTextures(1, &texture);
      return result;
   }
   void publish() {
      if (floorLock < 0)
         return;
      ++serial;
      Json state = {
          {"version", 1},
          {"timestamp",
           std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()},
          {"enabled", bool(floor.value()) && !paused},
          {"pid", getpid()},
          {"serial", serial},
          {"width", FloorW},
          {"height", FloorH},
          {"meters", sample["meters"]}};
      if (floor.value() && !paused) {
         auto pixels = floorPixels();
         std::string bytes = "HTPF";
         for (int i = 0; i < 8; ++i)
            bytes.push_back(char(serial >> (i * 8)));
         bytes.append(reinterpret_cast<const char *>(pixels.data()), pixels.size());
         atom(runtime / "htop-floor.rgba", bytes.data(), bytes.size());
      }
      auto json = state.dump();
      atom(runtime / "htop-floor.json", json.data(), json.size());
   }

 public:
   int selected = 0;
   Monitor(const std::string &screenshot) : dx::GpuWindow(1152, 760, "htop"), capture(screenshot) {
      xclass("org.domed.Htop");
      if (auto dir = getenv("XDG_RUNTIME_DIR")) {
         runtime = dir;
         auto record =
             runtime / (std::filesystem::path(getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "")
                            .filename()
                            .string() +
                        ".dome-session.json");
         try {
            std::ifstream in(record);
            Json j;
            in >> j;
            xrOnly = j.value("realm", "") == "xr";
         } catch (...) {
         }
         struct stat st{};
         if (!stat(runtime.c_str(), &st) && st.st_uid == getuid() && S_ISDIR(st.st_mode) &&
             !(st.st_mode & 0022)) {
            floorLock =
                open((runtime / "htop-floor.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (floorLock >= 0 && flock(floorLock, LOCK_EX | LOCK_NB)) {
               close(floorLock);
               floorLock = -1;
            }
         }
      }
      if (xrOnly) {
         summary.hide();
         info.resize(16, 8, 1120, 32);
         filter.resize(76, 48, 610, 32);
         floor.resize(710, 48, 220, 32);
         pause.resize(954, 48, 150, 32);
         processes.resize(16, 96, 1120, 640);
      }
      floor.value(1);
      floor.tooltip("CPU, RAM and swap bars on DomeD's real STAGE floor, alongside cmuxr");
      if (floorLock < 0) {
         floor.deactivate();
         floor.tooltip("Another window owns the floor, or no private runtime directory is available");
      }
      filter.when(FL_WHEN_CHANGED);
      filter.callback([](Fl_Widget *, void *p) { static_cast<Monitor *>(p)->rebuild(); }, this);
      pause.callback([](Fl_Widget *, void *p) { static_cast<Monitor *>(p)->command("Pause sampling"); },
                     this);
      floor.callback([](Fl_Widget *, void *p) { static_cast<Monitor *>(p)->publish(); }, this);
      for (auto entry : {"File/Quit", "View/Refresh", "View/Pause sampling", "View/CPU sort",
                         "View/Memory sort", "View/PID sort", "Process/Terminate (SIGTERM)",
                         "Process/Stop (SIGSTOP)", "Process/Continue (SIGCONT)"})
         menu.add(entry, 0, menuAction, this);
      menu.hide();
      desktop = std::make_unique<DesktopMenu>(*this, menu);
      end();
      resizable(processes);
      size_range(800, 480);
      dx::style(this);
      callback([](Fl_Widget *w, void *) { w->hide(); });
      refresh();
      Fl::add_timeout(.2, tick, this);
   }
   ~Monitor() {
      Fl::remove_timeout(tick, this);
      if (floorLock >= 0) {
         std::error_code error;
         std::filesystem::remove(runtime / "htop-floor.json", error);
         std::filesystem::remove(runtime / "htop-floor.rgba", error);
         close(floorLock);
      }
   }
   void rebuild() {
      processes.data = hgui::visible(sample, filter.value(), processes.sort, processes.reverse);
      processes.rows(processes.data.size());
      processes.select_all_rows(0);
      for (size_t i = 0; i < processes.data.size(); ++i)
         if (processes.data[i]["pid"] == selected)
            processes.select_row(i);
      processes.redraw();
   }
   void refresh() {
      sample = hgui::snapshot();
      summary.sample = sample;
      summary.redraw();
      std::ostringstream line;
      line << sample["tasks"] << " tasks · " << sample["running"] << " running · " << sample["cpus"]
           << " CPUs · Load " << sample["load"][0] << "  " << sample["load"][1] << "  " << sample["load"][2];
      if (xrOnly)
         line << " · performance bars on XR floor";
      info.copy_label(line.str().c_str());
      rebuild();
      if (shown())
         publish();
   }
   void command(const std::string &name) {
      if (name == "Quit")
         hide();
      else if (name == "Pause sampling") {
         paused = !paused;
         pause.label(paused ? "Resume" : "Pause");
         publish();
      } else if (name == "Refresh")
         refresh();
      else if (name == "CPU sort" || name == "Memory sort" || name == "PID sort") {
         processes.sort = name == "CPU sort" ? 6 : name == "Memory sort" ? 7 : 0;
         processes.reverse = processes.sort != 0;
         rebuild();
      } else if (name.find("SIG") != std::string::npos) {
         int fd = HtopGui_openTarget(selected);
         if (fd < 0) {
            fl_alert("Process unavailable: %s", strerror(errno));
            return;
         }
         refresh();
         auto found = std::find_if(processes.data.begin(), processes.data.end(),
                                   [&](const Json &p) { return p["pid"] == selected; });
         std::string command = found == processes.data.end() ? std::to_string(selected)
                                                             : (*found)["command"].get<std::string>();
         int sig = name.find("SIGTERM") != std::string::npos   ? SIGTERM
                   : name.find("SIGSTOP") != std::string::npos ? SIGSTOP
                                                               : SIGCONT;
         if (fl_choice("%s\nPID %d: %.250s", "Cancel", "Send signal", nullptr, name.c_str(), selected,
                       command.c_str()) == 1) {
            int error = HtopGui_signalTarget(fd, sig);
            if (error)
               fl_alert("Signal failed: %s", strerror(error));
         }
         close(fd);
      }
   }
   void save(const std::string &path) {
      make_current();
      draw();
      int width = pixel_w(), height = pixel_h();
      std::vector<unsigned char> raw(width * height * 3);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, raw.data());
      std::ofstream out(path, std::ios::binary);
      out << "P6\n" << width << " " << height << "\n255\n";
      for (int y = height - 1; y >= 0; --y)
         out.write(reinterpret_cast<const char *>(&raw[y * width * 3]), width * 3);
   }
   int handle(int event) override {
      if (event == FL_KEYDOWN || event == FL_SHORTCUT) {
         if (Fl::event_key() == FL_F + 5) {
            refresh();
            return 1;
         }
         if (Fl::event_key() == FL_F + 9) {
            command("Terminate (SIGTERM)");
            return 1;
         }
         if (Fl::event_key() == 'q' && Fl::event_state(FL_CTRL)) {
            hide();
            return 1;
         }
      }
      return dx::GpuWindow::handle(event);
   }
};
Processes::Processes(int x, int y, int w, int h, Monitor *window) : Fl_Table_Row(x, y, w, h), owner(window) {
   cols(9);
   col_header(1);
   col_header_height(32);
   row_height_all(28);
   col_resize(1);
   type(SELECT_SINGLE);
   when(FL_WHEN_RELEASE);
   int widths[] = {80, 110, 60, 60, 110, 60, 90, 90, 340};
   for (int i = 0; i < 9; ++i)
      col_width(i, widths[i]);
   callback([](Fl_Widget *w, void *) {
      auto self = static_cast<Processes *>(w);
      int row = self->callback_row(), col = self->callback_col();
      if (self->callback_context() == CONTEXT_COL_HEADER && col >= 0) {
         if (self->sort == col)
            self->reverse = !self->reverse;
         else {
            self->sort = col;
            self->reverse = col == 6 || col == 7;
         }
         self->owner->rebuild();
      } else if (row >= 0 && row < int(self->data.size()))
         self->owner->selected = self->data[row]["pid"];
   });
   end();
}
void Processes::draw_cell(TableContext context, int row, int column, int x, int y, int w, int h) {
   static const char *headers[] = {"PID", "User", "PRI", "NI", "RES", "State", "CPU %", "MEM %", "Command"};
   if (context == CONTEXT_STARTPAGE) {
      fl_font(dx::font, dx::size);
      return;
   }
   if (context != CONTEXT_CELL && context != CONTEXT_COL_HEADER)
      return;
   fl_push_clip(x, y, w, h);
   fl_color(context == CONTEXT_COL_HEADER ? FL_BACKGROUND_COLOR
            : row_selected(row)           ? FL_SELECTION_COLOR
                                          : FL_BACKGROUND2_COLOR);
   fl_rectf(x, y, w, h);
   fl_color(FL_FOREGROUND_COLOR);
   std::string text;
   if (context == CONTEXT_COL_HEADER)
      text = headers[column];
   else if (row >= 0 && row < int(data.size())) {
      auto &p = data[row];
      std::ostringstream out;
      switch (column) {
      case 0:
         out << p["pid"];
         break;
      case 1:
         out << p["user"].get<std::string>();
         break;
      case 2:
         out << p["priority"];
         break;
      case 3:
         out << p["nice"];
         break;
      case 4:
         out << std::fixed << std::setprecision(1) << p["resident_kib"].get<double>() / 1024 << " MiB";
         break;
      case 5:
         out << p["state"].get<std::string>();
         break;
      case 6:
         out << std::fixed << std::setprecision(1) << p["cpu"].get<double>();
         break;
      case 7:
         out << std::fixed << std::setprecision(1) << p["memory"].get<double>();
         break;
      case 8:
         out << p["command"].get<std::string>();
         break;
      }
      text = out.str();
      for (char &c : text)
         if ((unsigned char)c < 32)
            c = ' ';
   }
   // Clip before GL text rasterization: a command line can exceed Cairo's
   // maximum texture width even when the resulting texture would be clipped.
   if (!text.empty() && w > 10) {
      auto count = std::min<glong>(g_utf8_strlen(text.c_str(), -1), w);
      text.resize(g_utf8_offset_to_pointer(text.c_str(), count) - text.c_str());
      while (!text.empty() && fl_width(text.c_str()) > w - 10) {
         auto previous = g_utf8_find_prev_char(text.c_str(), text.c_str() + text.size());
         text.resize(previous ? previous - text.c_str() : 0);
      }
      if (!text.empty())
         fl_draw(text.c_str(), x + 5, y, w - 10, h, FL_ALIGN_LEFT | FL_ALIGN_INSIDE, nullptr, 0);
   }
   fl_pop_clip();
}
int main(int argc, char **argv) try {
   bool dump = false;
   std::string screenshot;
   for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--dump-json")
         dump = true;
      else if (arg == "--screenshot" && i + 1 < argc)
         screenshot = argv[++i];
      else if (arg == "--help") {
         puts("htop-fltk: graphical upstream htop collector\n--dump-json  sample without a "
              "display\n--screenshot FILE.ppm  capture after two samples\nConsole htop arguments remain "
              "available through htop --tui.");
         return 0;
      } else {
         fprintf(stderr, "Unknown graphical option: %s (use htop --tui for console options)\n", arg.c_str());
         return 2;
      }
   }
   if (HtopGui_init())
      return 1;
   if (dump) {
      hgui::snapshot();
      usleep(250000);
      std::cout << hgui::snapshot().dump() << '\n';
      HtopGui_done();
      return 0;
   }
   if (getenv("WAYLAND_DISPLAY"))
      setenv("FLTK_BACKEND", "wayland", 1);
   dx::init();
   {
      Monitor window(screenshot);
      window.show();
      Fl::run();
   }
   HtopGui_done();
   return 0;
} catch (const std::exception &e) {
   fprintf(stderr, "htop-fltk: %s\n", e.what());
   return 1;
}
