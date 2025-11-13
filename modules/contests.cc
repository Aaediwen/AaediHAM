#include "../aaediclock.h"
#include "../utils.h"
#include "contests.h"
#include <sstream>

SDL_TimerID contest_timer = 0;
std::vector<struct contest> contest_feed;

void parse_contests(char* xml) {
    struct contest temp;
    std::istringstream stream(xml);
    std::string keyline;
    size_t tag_start, tag_stop;
    bool in_item;
    while (std::getline(stream, keyline)) {
        tag_start=keyline.find("<item>");
        tag_stop=keyline.find("</item>");
        if ( tag_start != std::string::npos ) {
            temp.title.clear();
            temp.link.clear();
            temp.description.clear();
            temp.guid.clear();
            in_item = true;
        }
        if ( tag_stop != std::string::npos) {
            if (in_item) {
                // close out and store here
                bool found = false;
                if (!contest_feed.empty()) {
                    for (auto contest : contest_feed) {
                        if (contest.guid == temp.guid) {
                            found = true;
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
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=7;
                temp.title = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
        tag_start=keyline.find("<link>");
        tag_stop=keyline.find("</link>");
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=6;
                temp.link = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
        tag_start=keyline.find("<description>");
        tag_stop=keyline.find("</description>");
        if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
            if (in_item) {
                tag_start +=13;
                temp.description = keyline.substr(tag_start, tag_stop - tag_start);
            }
        }
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

void fetch_contests () {
     char* fetch_spots = 0 ;
     Uint32 data_size = 0;

     debug_log <<"CONTESTS: Fetching Spots from WA7BNM via timer\n";
     SDL_Log("Fetching contests from WA7BNM via timer");
     data_size = http_loader("https://www.contestcalendar.com/calendar.rss", (void**)&fetch_spots);

     if (data_size) {
          parse_contests(fetch_spots);
//          add_data_cache(MOD_CONTESTS, blob.length(), (void*)blob.data());
          if(fetch_spots) {
               free (fetch_spots);
               fetch_spots=0;
          }
     }
     return;
}


Uint32 SDLCALL fetch_contests (void *userdata, SDL_TimerID timerID, Uint32 interval) {
     if (timerID) {
          fetch_contests();
          return (3600000); // 1 Hr
     } else {
          return 0;
     }
}


void contest_module(ScreenFrame& panel) {
     char* fetch_spots = 0 ;
     Uint32 data_size = 0;
     time_t cache_time;
     SDL_FRect TextRect;
     SDL_Color panel_color = {0, 128, 128, 255};
     if (!contest_timer) {
          contest_timer = SDL_AddTimer(30, fetch_contests, NULL);
     }
//     data_size = cache_loader(MOD_CONTESTS, (void**)&fetch_spots, &cache_time);
     panel.Clear();

     float unitx = panel.dims.w/20;
     float unity = panel.dims.h/20;
     TextRect.w=unitx*18;
     TextRect.h=unity;
     TextRect.x=unitx;
     TextRect.y=2;
     panel.render_text(TextRect, Sans, panel_color, "WA7BNM Contest Calendar");
     TextRect.y += unity;
     if (!contest_feed.empty()) {
         for (auto contest : contest_feed) {
             if (TextRect.y <= unity*19) {
                 TextRect.x = 2;
                 TextRect.w = unitx*9;
                 panel.render_text(TextRect, Sans, panel_color, contest.title.c_str());
                 TextRect.x = unitx*10;
                 panel.render_text(TextRect, Sans, panel_color, contest.description.substr(0, 32));
                 TextRect.y += unity;
             }
         }
     }
     return;
}