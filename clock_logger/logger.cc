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
std::vector<std::string> itterate_called_modules;
void handle_itterate (std::string& line) {
  size_t label_start, label_stop;
  std::string substr;
  label_start=line.find(":");
    substr = line.substr(label_start+1, (label_stop - label_start));
    if (substr.find("Calling ") != std::string::npos) {
        itterate_called_modules.push_back(substr);
  } else if (substr.find("Module Timer") != std::string::npos) {
        int ms = 0;
        size_t ms_start = substr.find_first_of("0123456789");
        ms = std::stoi(substr.substr(ms_start));
        int mod_ms = ms - last_ms;
        substr += " -- "+ std::to_string(mod_ms) + "ms";
        itterate_called_modules.push_back(substr);
        last_ms = ms;

  } else if (substr.find("Took") != std::string::npos) {
      int ms = 0;
      size_t ms_start = substr.find_first_of("0123456789");
      ms = std::stoi(substr.substr(ms_start));
      if (ms > 100) {
           wmove(main_window, 25,2);
           substr = line.substr(label_start+2, std::string::npos);
           wclrtoeol(main_window);
           wmove(main_window, 25,2);
           wprintw(main_window, substr.c_str());
           int module_index = 26;
           for (auto& module_line : itterate_called_modules) {
             wmove(main_window, module_index,2);
             wclrtoeol(main_window);
             wmove(main_window, module_index,2);
             wprintw(main_window, module_line.c_str());
             module_index++;
          }
          wmove(main_window, module_index,2);
          wprintw(main_window, "                                                           ");
      }
      itterate_called_modules.clear();
      refresh();
  }
  return;
}

void handle_resize(std::string& line) {
  size_t label_start, label_stop;
  std::string substr;
  label_start=line.find(":");
  label_stop = line.find(":", label_start+1);
  substr = line.substr(label_start+1, (label_stop - label_start));
  wmove(main_window, LINES-6,2);
  std::cout << substr.c_str() << "\n";
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
      handle_itterate(linestring);
      typeline = 3;
    } else if (header == "RSS") {
      typeline = 4;
    }  else if (header == "SCREENFRAME") {
      typeline = 5;
    }  else if (header == "RESIZE") {
    handle_resize(linestring);
      typeline = 6;
    }  else if (header == "INIT") {
      typeline = 7;
    }  else if (header == "POTA") {
      typeline = 8;
    }   else if (header == "LUNAR") {
      typeline = 9;
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
    }else if (header == "MAP") {
      typeline = 14;
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
    } else if (header == "SOLAR") {
      typeline = 19;
      ignore_line = true;
    } else if (header == "Time") {
      typeline = 20;
    }else if (header == "EVENT") {
      typeline = LINES-3;
    } else if (header == "EXIT") {
      typeline = LINES-2;
    }

    else {
      typeline = 2;
      ignore_line = true;
    }
    if (!ignore_line) {
         wmove (main_window, typeline,2);
         wclrtoeol(main_window);
         wmove (main_window, typeline,2);
         wprintw(main_window, linetemp);
         refresh();
    }
    commandkey = getch();
    flushinp();
  }
  endwin();
  return 0;
}