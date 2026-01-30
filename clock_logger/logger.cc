#include <fstream>
#include <algorithm>
#include <iostream>
#include <vector>
#include <ncurses.h>
#define main_window stdscr

void ltrim(std::string &s) {
    auto it = std::find_if(s.begin(), s.end(),
        [](unsigned char c){ return !std::isspace(c); });
    s.erase(s.begin(), it);
}

int last_ms =0;
int module_id=1000;
std::vector<std::string> itterate_called_modules;
std::vector<int>module_times[30];
std::string module_names[30];

void handle_itterate (const std::string& line) {
   if (line.find("Calling ") != std::string::npos) {
       size_t id_start = line.find("(");
       size_t id_stop = line.find(")");
       if (id_start != std::string::npos) {
         if (id_stop != std::string::npos) {
            if (id_stop > id_start) {
               size_t id_len = id_stop - id_start;
               std::string id_string = line.substr(id_start+1, id_len-1);
               module_id = std::stoi(id_string);
               itterate_called_modules.push_back(line);
               module_names[module_id] = line.substr(0, id_stop);
//               wmove(main_window, 25, 25);
//               wprintw(main_window, id_string.c_str());
            }
         }
       }
   }  else if (line.find("Module Timer") != std::string::npos) {
       // ITTERATE: Module Timer RSS -- 21 MIlliseconds
        int aggrigate_ms = 0;
        int module_ms =0;
        size_t ms_start = line.find_first_of("0123456789");
        size_t ms_stop = line.find(" ",ms_start);
        if (ms_start != std::string::npos) {
            if (ms_stop != std::string::npos) {
                aggrigate_ms = std::stoi(line.substr(ms_start, (ms_stop - ms_start)));
                module_ms = aggrigate_ms - last_ms;
                if (module_id < 30) {
                     module_times[module_id].push_back(module_ms);
                }
                if (module_times[module_id].size() > 50) {
                    module_times[module_id].erase(module_times[module_id].begin());
                }
                last_ms = aggrigate_ms;
            }
        }
   }  else if (line.find("Took") != std::string::npos) {
      int module_index = 26;
      last_ms =0;
      module_id = 1000;
      for (int c = 0 ; c < 30 ; c++) {
           if (!(module_times[c].empty())) {
               int sum = 0;
               std::string tempstring;
               wmove (main_window, module_index,2);
//               tempstring = std::to_string(c);
               tempstring = module_names[c];
               wprintw(main_window, tempstring.c_str());
               wmove (main_window, module_index,40);
               tempstring = std::to_string(module_times[c].size());
               wprintw(main_window, tempstring.c_str());
               wmove (main_window, module_index,50);
               for (int& foo: module_times[c]) {
                     sum += foo;
               }
               tempstring = std::to_string(sum/module_times[c].size());
               wprintw(main_window, tempstring.c_str());
               sum = 0;
               module_index++;
           }
      }
      itterate_called_modules.clear();
      refresh();
   }
  return;
}

struct panel_info {
   std::string name;
   std::string size;
   std::string dims;
   std::string pointer;
};

void handle_resize(std::string& line) {
  size_t label_start, label_stop;
  std::string substr;
  label_start=line.find("RESIZE");
  label_stop = line.find(":", label_start+1);
  substr = line.substr(label_start+1, (label_stop - label_start));
  wmove(main_window, LINES-6,2);
//  std::cout << substr.c_str() << "\n";
  if (substr.find("Driver") != std::string::npos) {
      wmove(main_window, LINES-5,2);
      substr = line.substr(label_start+2, std::string::npos);
      wprintw(main_window, substr.c_str());
      refresh();
  } else if (substr.find("Renderer Max Texture Size") != std::string::npos) {
      wmove(main_window, LINES-6,2);
      wclrtoeol(main_window);
      wmove(main_window, LINES-6,2);
      substr = line.substr(label_start+2, std::string::npos);
      wprintw(main_window, substr.c_str());
      refresh();
  } else if (substr.find("Resizing Window to") != std::string::npos) {
      wmove(main_window, LINES-7,2);
      wclrtoeol(main_window);
      wmove(main_window, LINES-7,2);
      substr = line.substr(label_start+2, std::string::npos);
      wprintw(main_window, substr.c_str());
      refresh();
  } else if (substr.find("SCREENFRAME:") != std::string::npos) {

  }
  return;
}

int main (int argc, char* argv[]) {
  int commandkey = 0;
  std::ifstream configfile("clock_debug.log");
  initscr();
  nodelay(main_window, TRUE);
  cbreak();
  scrollok(main_window,FALSE);
  noecho();
  nonl;
  wresize (main_window, LINES, COLS);
  wborder(main_window, 0,0,0,0,0,0,0,0);
  wmove(main_window, 2,2);
  wprintw (main_window, "Test Text");
  bool ignore_line = false;
  while (commandkey != 'q') {
    ignore_line = false;
    char linetemp[1024];
    std::string header;
    header.clear();
    std::string linestring;
    configfile.getline(linetemp, 1024);
    linestring = linetemp;
    size_t header_stop = linestring.find(":");
    header = linestring.substr(0,header_stop);
    int typeline =0;
    ltrim(header);
    if (header == "ITTERATE") {
      handle_itterate(linestring.substr(header_stop, std::string::npos));
      typeline = 3;
      ignore_line=true;
    } else if (header == "RSS") {
      typeline = 4;
    }  else if (header == "SCREENFRAME") {
      typeline = 5;
    }  else if (header == "RESIZE") {
      handle_resize(linestring);
      ignore_line=true;
      typeline = 6;
    }  else if (header == "INIT") {
      typeline = 7;
    }  else if (header == "POTA") {
      typeline = 8;
    }   else if (header == "LUNAR") {
      typeline = 9;
      ignore_line=true;
    }  else if (header == "CLOCK") {
      typeline = 10;
    }else if (header == "SAT TRACKER") {
      typeline = 11;
      ignore_line = true;
    } else if (header == "SAT_TRACKER") {
      typeline = 11;
      ignore_line = true;
    }else if (header == "DXSPOTS") {
      typeline = 12;
    }else if (header == "CACHE") {
      typeline = 13;
      ignore_line=true;
    }else if (header == "MAP") {
      typeline = 14;
      ignore_line=true;
    } else if (header == "OVERLAY") {
      typeline = 15;
      ignore_line = true;
    } else if (header == "WSPR") {
      typeline = 16;
      ignore_line = true;
    }  else if (header == "NCDXF") {
      typeline = 17;
      ignore_line = true;
    } else if (header == "KINDEX") {
      typeline = 18;
      ignore_line=true;
    } else if (header == "SOLAR") {
      typeline = 19;
      ignore_line = true;
    } else if (header == "Time") {
      typeline = 20;
    }else if (header == "EVENT") {
      typeline = LINES-3;
    } else if (header == "EXIT") {
      typeline = LINES-2;
    } else {
      typeline = 2;
      ignore_line = true;
    }
    ignore_line = true;
    if (!ignore_line) {
         wmove (main_window, typeline,2);
         wclrtoeol(main_window);
         wmove (main_window, typeline,2);
         wprintw(main_window, linetemp);
         refresh();
    }
    commandkey = getch();
    wborder(main_window, 0,0,0,0,0,0,0,0);
    flushinp();
  }
  endwin();
  return 0;
}