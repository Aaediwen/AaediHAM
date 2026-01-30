#include "../aaediclock.h"
#include "../utils.h"
#include "contests.h"
#include <sstream>

SDL_TimerID contest_timer = 0;
std::vector<struct contest> contest_feed;

void parse_contests(char* xml) {
    if (!xml || ! xml[0]) {
         return;
    }
    contest_feed.clear();
    struct contest temp;
    temp.title.clear();
    temp.link.clear();
    temp.description.clear();
    temp.guid.clear();
    std::istringstream stream(xml);
    std::string keyline;
    keyline.clear();
    size_t tag_start, tag_stop;
    tag_start = 0;
    tag_stop = 0;
    bool in_item = false;
    while (std::getline(stream, keyline)) {
        tag_start=keyline.find("<item>");
        tag_stop=keyline.find("</item>");
        // new item entry
        if ( tag_start != std::string::npos ) {
            temp.title.clear();
            temp.link.clear();
            temp.description.clear();
            temp.guid.clear();
            in_item = true;
        }
        // end item entry
        if ( tag_stop != std::string::npos) {
            if (in_item) {
                // close out and store here
                bool found = false;
                if (!contest_feed.empty()) {
                    for (const auto& contest : contest_feed) {
                        if (contest.guid == temp.guid) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    contest_feed.push_back(temp);
                }
            }
            in_item = false;
        }
        tag_start=keyline.find("<title>");
        tag_stop=keyline.find("</title>");
        // contest title
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=7;
                temp.title = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
        // contest link
        tag_start=keyline.find("<link>");
        tag_stop=keyline.find("</link>");
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=6;
                temp.link = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
        // description
        tag_start=keyline.find("<description>");
        tag_stop=keyline.find("</description>");
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=13;
                temp.description = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
        // guid
        tag_start=keyline.find("<guid>");
        tag_stop=keyline.find("</guid>");
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=6;
                temp.guid = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
    }
    return;
}

int SDLCALL fetch_contests (void* data) {
     (void)data;
     char* fetch_spots = 0 ;
     Uint64 data_size = 0;

     debug_log <<"CONTESTS: Fetching Spots from WA7BNM via timer\n";
     SDL_Log("Fetching contests from WA7BNM via timer");
     data_size = http_loader("https://www.contestcalendar.com/calendar.rss", (void**)&fetch_spots);
     SDL_LockMutex(mutexes[MUTEX_CONTESTS]);
     if (data_size) {
          parse_contests(fetch_spots);
     }
     SDL_UnlockMutex(mutexes[MUTEX_CONTESTS]);
     if(fetch_spots) {
          free (fetch_spots);
          fetch_spots=0;
     }
     SDL_Delay(10000);
     return 0;
}


Uint32 SDLCALL fetch_contests (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(fetch_contests, "Contest Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "Failed to Create Contest Fetch Thread\n";
          }
          return (3600000); // 1 Hr
     } else {
          return 0;
     }
}

size_t contest_page[2]={0,2};
bool cycle = false;
void contest_module(ScreenFrame& panel) {
//    const size_t contest_start = contest_page[0]*15;
    size_t contest_index = 0;
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Contest Module during resize event!");
        return ;
    }
    if (!Sans) {
        debug_log << "CONTESTS: No font defined\n";
        return ;
    }
    if (!panel.GetRenderer()) {
        debug_log << "CONTESTS: Missing Renderer!\n";
        return ;
    }
    if (!panel.texture) {
        debug_log << "CONTESTS: Missing PANEL!\n";
        return ;
    }
    if (clock_mouse_event.mod_owner == MOD_CONTESTS) {
        SDL_Log ("Click event in Contest module at %f, %f", clock_mouse_event.mod_cords.x, clock_mouse_event.mod_cords.y);
        cycle = !cycle;
        if (!cycle) {
             contest_page[0]=0;
        }
        clock_mouse_event.mod_owner = MOD_NULL;
    }

     SDL_FRect TextRect;
     SDL_Color panel_color;
     if (cycle) {
         panel_color = {0, 128, 128, 255};
     } else {
         panel_color = {0, 128, 200, 255};
     }
     if (!contest_timer) {
          contest_timer = SDL_AddTimer(30, fetch_contests, NULL);
     }
     panel.Clear();

     float unitx = panel.dims.w/20;
     float unity = panel.dims.h/15;
     TextRect.w=unitx*18;
     TextRect.h=unity;
     TextRect.x=unitx/2;
     TextRect.y=2;
     panel.render_text(TextRect, Sans, panel_color, "WA7BNM Contest Calendar");
     TextRect.y += unity;
     SDL_LockMutex(mutexes[MUTEX_CONTESTS]);

     if (!contest_feed.empty()) {
         for (const auto& contest : contest_feed) {
             if ((TextRect.y <= unity*15) && (contest_index >= contest_page[0]*15) && (contest_index<(contest_page[0]*15)+15)) {
                 TextRect.x = 2;
                 TextRect.w = unitx*10;
                 if (!contest.title.empty()) {
                      panel.render_text(TextRect, Sans, panel_color, contest.title.c_str());
                 }
                 TextRect.x = unitx*11;
                 TextRect.w = unitx*8;
                 if (!contest.description.empty()) {
                      std::string tempdesc = contest.description;
                      size_t resize_point = tempdesc.size();
                      if (resize_point > 32) { resize_point = 32; }
                      tempdesc.resize(resize_point);
                      panel.render_text(TextRect, Sans, panel_color, tempdesc);
                 }
                 TextRect.y += unity;
             }
             contest_index++;
         }
         if (cycle) {
            contest_page[1]++;
            if (contest_page[1] > 10) {
                contest_page[0]++;
                contest_page[1]=0;
                if (contest_page[0] > (contest_feed.size()/15)) {
                    contest_page[0]=0;
                }
            }
         }
     }
     SDL_UnlockMutex(mutexes[MUTEX_CONTESTS]);

     return;
}