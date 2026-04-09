#include "aaediclock.h"
#include "utils/http_fetch.h"
#include "contests.h"
#include <algorithm>
#include <sstream>
#include <mutex>
#include <libxml/tree.h>

SDL_TimerID contest_timer = 0;
std::vector<struct contest> contest_feed;
std::mutex contest_mutex;
aaediclock_host_api* host_api = nullptr;
/*
void parse_contests(char* xml) {
    if (!xml || ! xml[0]) {
         return;
    }
    const std::lock_guard<std::mutex>contest_lock(contest_mutex);
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
*/
struct contest temp;
bool in_item = false;
void parse_contests(xmlNode* start_node) {
     xmlNode* current_node = nullptr;
     for (current_node = start_node; current_node; current_node = current_node->next) {
          if (current_node->type == XML_ELEMENT_NODE) {
               std::string NodeName(reinterpret_cast<const char*>(current_node->name));
//               *(host_api->AaediHAM_LogDebug) << "XML Node Name: "<< NodeName << "\n";
               std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
               if ((NodeName == "rss") || NodeName == "channel") {
                    parse_contests(current_node->children);
               }
               if (NodeName == "item") {
                    temp.title.clear();
                    temp.link.clear();
                    temp.description.clear();
                    temp.guid.clear();
                    in_item = true;
                    parse_contests(current_node->children);
                    in_item = false;
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
               } else if (NodeName == "title") {
                    if (in_item) {
                         std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                         temp.title = xml_content;
                    }
               } else if (NodeName == "link") {
                    if (in_item) {
                         std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                         temp.link = xml_content;
                    }
               } else if (NodeName == "description") {
                    if (in_item) {
                         std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                         temp.description = xml_content;
                    }
               } else if (NodeName == "guid") {
                    if (in_item) {
                         std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                         temp.guid = xml_content;
                    }
               }
          }
     }
}

int SDLCALL fetch_contests (void* data) {
     (void)data;
     char* fetch_spots = 0 ;
     Uint64 data_size = 0;

     *(host_api->AaediHAM_LogDebug) <<"CONTESTS: Fetching Spots from WA7BNM via timer\n";
     SDL_Log("Fetching contests from WA7BNM via timer");
     data_size = http_loader("https://www.contestcalendar.com/calendar.rss", (void**)&fetch_spots);
     if (data_size) {
          xmlDocPtr xml_tree = 0;
          xml_tree = xmlReadMemory(fetch_spots, static_cast<int>(data_size), nullptr, nullptr, 0);
          if (!xml_tree) {
               *(host_api->AaediHAM_LogDebug) << "Failed to parse Context Feed XML\n";
          } else {
               parse_contests(xmlDocGetRootElement(xml_tree));
               xmlFreeDoc (xml_tree);
               xml_tree = nullptr;
          }
//          parse_contests(fetch_spots);
     }
     if(fetch_spots) {
          free (fetch_spots);
          fetch_spots=0;
     }
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
              *(host_api->AaediHAM_LogDebug) << "Failed to Create Contest Fetch Thread\n";
          }
          return (3600000); // 1 Hr
     } else {
          return 0;
     }
}





extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new contest_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void contest_plugin::plugin_init() const {
     if (!contest_timer) {
          contest_timer = SDL_AddTimer(30, fetch_contests, NULL);
     }
    return;
}

void contest_plugin::plugin_exit() const {
    if (contest_timer) {
       SDL_RemoveTimer(contest_timer);
    }
    return;
}
size_t contest_page[2]={0,2};
bool cycle = false;

void contest_plugin::plugin_main(const aaediclock_FRect& dims) const {
//    const size_t contest_start = contest_page[0]*15;
    if ((dims.h < 10) || (dims.w < 10)) {
        return;
    }
    size_t contest_index = 0;
    struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
    if (mouse_event.valid) {
        SDL_Log ("Click event in Contest module at %f, %f", mouse_event.coords.x, mouse_event.coords.y);
        cycle = !cycle;
        if (!cycle) {
             contest_page[0]=0;
        }
    }

     aaediclock_FRect TextRect;
     aaediclock_Color panel_color;
     if (cycle) {
         panel_color = {0, 128, 128, 255};
     } else {
         panel_color = {0, 128, 200, 255};
     }

     host_api->AaediHAM_GraphicsClear();

     float unitx = dims.w/20;
     float unity = dims.h/15;
     TextRect.w=unitx*18;
     TextRect.h=unity;
     TextRect.x=unitx/2;
     TextRect.y=2;
     host_api->AaediHAM_GraphicsDrawText("WA7BNM Contest Calendar", panel_color, TextRect);
     TextRect.y += unity;
     const std::lock_guard<std::mutex>contest_lock(contest_mutex);
     if (!contest_feed.empty()) {
         for (const auto& contest : contest_feed) {
             if ((TextRect.y <= unity*15) && (contest_index >= contest_page[0]*15) && (contest_index<(contest_page[0]*15)+15)) {
                 TextRect.x = 2;
                 TextRect.w = unitx*10;
                 if (!contest.title.empty()) {
                      host_api->AaediHAM_GraphicsDrawText(contest.title.c_str(), panel_color, TextRect);
                 }
                 TextRect.x = unitx*11;
                 TextRect.w = unitx*8;
                 if (!contest.description.empty()) {
                      std::string tempdesc = contest.description;
                      size_t resize_point = tempdesc.size();
                      if (resize_point > 32) { resize_point = 32; }
                      tempdesc.resize(resize_point);
                      host_api->AaediHAM_GraphicsDrawText(tempdesc.c_str(), panel_color, TextRect);
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

     return;


}

const char* contest_plugin::getName() const {
    return "Contest Module";
}

void contest_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

