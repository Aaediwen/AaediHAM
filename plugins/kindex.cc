#include "aaediclock.h"
//#include "core/utils.h"
#include "utils/http_fetch.h"
#include "kindex.h"
#include <deque>
#include <sstream>
#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#endif
#include <mutex>
using json = nlohmann::json;

struct KIndexPoint {
    float kindex;
    bool day_mark;
    time_t timestamp;
};

struct SolarWindPoint {
	float density;
	float speed;
	Uint32 temperature;
	bool day_mark;
	time_t timestamp;
};

std::vector<struct KIndexPoint>kindex_cache;
std::vector<struct SolarWindPoint>solar_wind_cache;

static nlohmann::json::iterator wind_index;
static nlohmann::json::iterator wind_end;
SDL_TimerID kindex_timer = 0;
std::mutex kindex_mutex;
aaediclock_host_api* host_api = nullptr;

time_t parse_time_tag(const std::string& time_tag) {
    std::tm tm = {};
    // Adjust the format to match your time string exactly
    // e.g., "2025-07-29 15:00:00.000"
    int matched = sscanf(time_tag.c_str(), "%d-%d-%d %d:%d:%d",
                         &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                         &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

    if (matched < 6) {
        return (time_t)(-1);  // or handle error appropriately
    }

    tm.tm_year -= 1900;  // tm_year is years since 1900
    tm.tm_mon  -= 1;     // tm_mon is months since January [0-11]

    // Use mktime to convert to time_t (UTC)
    return timegm(&tm);

}

void write_wind_cache(time_t max_timestamp) {
	time_t current_timestamp =0;
	while ((wind_index != wind_end) && (current_timestamp < max_timestamp)) {
	     struct SolarWindPoint new_node;
             std::string index_string;
             try {
                  index_string = (*wind_index)[0].get<std::string>();
//                  *(host_api->AaediHAM_LogDebug) << "KINDEX: Time String: " <<  index_string.c_str() << "\n";
                  new_node.timestamp = parse_time_tag(index_string);
                  current_timestamp = new_node.timestamp;
                  new_node.day_mark = false;
                  size_t space_pos = index_string.find(' ');
                  if (space_pos != std::string::npos && index_string.size() > space_pos + 5) {
                       std::string time_part = index_string.substr(space_pos + 1, 5);
                       if (time_part == "00:00") {
                            new_node.day_mark = true;
                       }
                  }
                  index_string = (*wind_index)[1].get<std::string>();
//                  *(host_api->AaediHAM_LogDebug) << "KINDES: Density String: " << index_string.c_str() << "\n";
                  new_node.density = std::stof(index_string);
                  index_string = (*wind_index)[2].get<std::string>();
//                  *(host_api->AaediHAM_LogDebug) << "KINDEX: Speed String: " << index_string.c_str() << "\n";
                  new_node.speed = std::stof(index_string);
                  index_string = (*wind_index)[3].get<std::string>();
//                  *(host_api->AaediHAM_LogDebug) << "KINDEX: Temp String: " << index_string.c_str()<< "\n";
                  new_node.temperature = std::stoi(index_string);
                  if (solar_wind_cache.empty() || new_node.timestamp > solar_wind_cache.back().timestamp) {
                      solar_wind_cache.push_back(new_node);
                  }
//                  raw_points.push_back(new_node);
             } catch (const std::exception& e) {
                  *(host_api->AaediHAM_LogDebug) << "KINDEX: Skipped Wind: " << e.what() << "\n";
             }

             wind_index++;
	}
	return;
}

void merge_json (const char* k_index_list, const char* solar_wind_list) {
     json k_list, solar_index;
     try {
          k_list = json::parse(k_index_list);
          solar_index = json::parse(solar_wind_list);
     } catch (const std::exception& e) {
         (void)e;
          *(host_api->AaediHAM_LogDebug) << "KINDEX: JSON Parse error reading Solar Data\n";
          return;
     }

     wind_index = solar_index.begin();
     wind_end   = solar_index.end();

     for (const auto& spot : k_list) {
          std::string index_string;
          struct KIndexPoint new_node;
          if (!spot.is_array() || spot.size() < 4) continue;
          try {
               index_string = spot[1].get<std::string>();
               new_node.kindex =  std::stof(index_string);
               index_string = spot[0].get<std::string>();
               new_node.timestamp = parse_time_tag(index_string);
               new_node.day_mark = false;
               size_t space_pos = index_string.find(' ');
               if (space_pos != std::string::npos && index_string.size() > space_pos + 5) {
                    std::string time_part = index_string.substr(space_pos + 1, 5);
                    if (time_part == "00:00") {
                         new_node.day_mark = true;
                    }
               }
               const std::lock_guard<std::mutex>kindex_lock(kindex_mutex);
               if (kindex_cache.empty() || (new_node.timestamp > kindex_cache.back().timestamp)) {
                   write_wind_cache(new_node.timestamp);
                   kindex_cache.push_back(new_node);
               }
          } catch (const std::exception& e) {
               *(host_api->AaediHAM_LogDebug) << "KINDEX: Skipped Kindex: " << e.what() << "\n";
          }
     }
     return;
}


int SDLCALL fetch_kindex (void* data) {
     (void)data;
     Uint64 data_size;
     char* k_index_list = 0 ;
     char* solar_wind_list = 0;
//     std::string merged;
     SDL_Log ("Fetching Solar Weather from NOAA via timer");
     *(host_api->AaediHAM_LogDebug) << "KINDEX: Kindex cache Miss fetching data from NOAA via timer\n";
     data_size = http_loader("https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json", (void**)&k_index_list);   // live
     data_size += http_loader("https://services.swpc.noaa.gov/products/solar-wind/plasma-7-day.json", (void**)&solar_wind_list);
     *(host_api->AaediHAM_LogDebug) << "KINDEX: Fetched Sources\n";
     if (data_size) {
          merge_json(k_index_list, solar_wind_list);
//          add_data_cache(MOD_KINDEX, merged.length(), (void*)merged.data());
          if (k_index_list) {
              free (k_index_list);
              k_index_list = 0;
          }
          if (solar_wind_list) {
              free (solar_wind_list);
              solar_wind_list = 0;
          }
     }
     time_t cutoff = time(NULL) - 2600000;
     if (!kindex_cache.empty()) {
        for (size_t c = kindex_cache.size() ; c-- > 0 ;) {
            if ((kindex_cache[c].timestamp) < cutoff) {
                kindex_cache.erase(kindex_cache.begin()+c);
            }
        }
    }
    if (!solar_wind_cache.empty()) {
        for (size_t c = solar_wind_cache.size() ; c-- > 0 ;) {
            if ((solar_wind_cache[c].timestamp) < cutoff) {
                solar_wind_cache.erase(solar_wind_cache.begin()+c);
            }
        }
    }
    return 0;
}

Uint32 SDLCALL fetch_kindex (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
  if (timerID) {
    SDL_Thread* thread = SDL_CreateThread(fetch_kindex, "Kindex Fetcher", nullptr);
    if (thread) {
      SDL_DetachThread(thread);
    } else {
      *(host_api->AaediHAM_LogDebug) << "Failed to Create Kindex Fetch Thread\n";
    }
    return (3600000); // 6 hrs
//      return (300000);  // 5 mins for testing
  } else {
    return 0;
  }
}



void plot_solar_wind (aaediclock_FRect dims, const std::vector<float>&wind_prime, const aaediclock_FRect& bar_box, const aaediclock_Color& color){
      const float y_height = dims.h/8;
      if (wind_prime.size()) {
        std::vector<aaediclock_FPoint> chart_pts;
        float point_width = bar_box.w/wind_prime.size();
        aaediclock_FPoint new_point;
        new_point.x = bar_box.x;
        for (auto solar_wind : wind_prime) {
          new_point.y = y_height + ((solar_wind/100)* y_height);
          chart_pts.push_back(new_point);
          new_point.x += point_width;
        }
          host_api->AaediHAM_GraphicsDrawLines(color, chart_pts.data(), static_cast<int>(chart_pts.size()));
      }
      return;
}


extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new kindex_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void kindex_plugin::plugin_init() const {
    if (!kindex_timer) {
      kindex_timer = SDL_AddTimer(30, fetch_kindex, NULL);
    }
    return;
}

void kindex_plugin::plugin_exit() const {
    if (kindex_timer) {
      SDL_RemoveTimer(kindex_timer);
    }
    return;
}

void kindex_plugin::plugin_main(const aaediclock_FRect& dims) const {

//    std::string merged;
//    std::istringstream data;
//    char* k_index_list = 0 ;

    if ((dims.w < 10) || (dims.h < 10)) {
        return;
    }
      aaediclock_FRect marker;
//    json source_json;
//    Uint64 data_size;
//    time_t cache_time;
//    std::string combined;
//    *(host_api->AaediHAM_LogDebug) << "KINDEX: Kindex checking cache\n";
//    data_size = cache_loader(MOD_KINDEX, (void**)&k_index_list, &cache_time);
//    if ((time(NULL) - cache_time) > 14400) {
//        if (k_index_list) {
//          free (k_index_list);
//          k_index_list = 0;
//        }
//    } else {
//        *(host_api->AaediHAM_LogDebug) << "KINDEX: Cache size: " << data_size << "\n";
//        merged.assign(k_index_list, static_cast<size_t>(data_size));
//        if (k_index_list) {
//          free (k_index_list);
//          k_index_list = 0;
//        }
//    }


//    data.clear();
//    *(host_api->AaediHAM_LogDebug) << "KINDEX: Cache Data size! " <<  merged.size() << "\n";
//    data.str(merged);

    // clear the box
    host_api->AaediHAM_GraphicsClear();
    host_api->AaediHAM_SetTarget();
//    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    aaediclock_FRect bar_box;
    *(host_api->AaediHAM_LogDebug) << "KINDEX: Rendering K index graph\n";
    std::vector<float>speed_prime;
    std::vector<float>density_prime;
    std::deque<float>speed_queue;
    std::deque<float>density_queue;
    float speed_old = 0;
    float density_old = 0;
    std::vector<SolarWindPoint>::iterator wind_index;
//    float klast, dlast, slast;
    float klast = 0.0;
    uint8_t type;
    size_t kindex_count;
    const std::lock_guard<std::mutex>kindex_lock(kindex_mutex);
    if (kindex_cache.empty() || solar_wind_cache.empty()) {
      *(host_api->AaediHAM_LogDebug) << "Missing Solar Data!\n";
      host_api->AaediHAM_GraphicsDrawText("MISSING KINDEX DATA", aaediclock_Color{128,128,128,0}, aaediclock_FRect {dims.w/20, dims.h/4, dims.h/10, (dims.w/10)*8});
      return;
    }
    wind_index = solar_wind_cache.begin();

    *(host_api->AaediHAM_LogDebug) << "KINDEX: Drawing chart size " << kindex_cache.size() << "\n";

    bar_box.x=1;
    bar_box.w = (dims.w-2)/kindex_cache.size();
    for (struct KIndexPoint& kindex : kindex_cache) {
      // reset the primes of actual solar wind display values
      speed_prime.clear();
      density_prime.clear();

      // render the K-index bar graph
      aaediclock_Color bar_color;
      klast = kindex.kindex;
      if (kindex.kindex < 1) {
          bar_color = {0, 128, 0, 255};
      } else if (kindex.kindex <2) {
          bar_color = {0, 128, 128, 255};
      } else if (kindex.kindex <3) {
          bar_color = {0, 0, 128, 255};
      } else if (kindex.kindex <4) {
          bar_color = {0, 0, 255, 255};
      } else if (kindex.kindex <5) {
          bar_color = {0, 255, 255, 255};
      } else if (kindex.kindex <6) {
          bar_color = {128, 128, 0, 255};
      } else if (kindex.kindex <7) {
          bar_color = {128, 0, 0, 255};
      } else if (kindex.kindex <8) {
          bar_color = {200, 0, 0, 255};
      } else {
          bar_color = {255, 0, 0, 255};
      }
      bar_box.h=((dims.h*.75f)/10.0f)*kindex.kindex;
      if (bar_box.h > dims.h) {
        bar_box.h = dims.h;
      }
      bar_box.y = dims.h - bar_box.h;
      host_api->AaediHAM_GraphicsDrawRect (bar_color, bar_box, 1);

      // process solar wind info
//      size_t wind_count;
//      data.read(reinterpret_cast<char*>(&wind_count), sizeof(wind_count));
        while (wind_index->timestamp < kindex.timestamp) {
           // populatethe queues and calc the primes
           speed_queue.push_back(speed_old - wind_index->speed);
           density_queue.push_back(density_old - wind_index->density);
           if (speed_queue.size() > 5) {
               speed_queue.pop_front();
           }
           speed_prime.push_back(std::accumulate(speed_queue.begin(), speed_queue.end(), 0.0f)/speed_queue.size());
           if (density_queue.size() > 5) {
               density_queue.pop_front();
           }
           density_prime.push_back(std::accumulate(density_queue.begin(), density_queue.end(), 0.0f)/density_queue.size());
           // update the 'previous' state
           speed_old = wind_index->speed;
           density_old = wind_index->density;
           wind_index++;
        }
              // plot the graphs for solar wind
      plot_solar_wind (dims, speed_prime, bar_box, aaediclock_Color{128,64,64,64});
      plot_solar_wind (dims, density_prime, bar_box, aaediclock_Color{64, 128, 64, 200});

      // -------- end processing wind info
      // prep for the next kindex

      if (kindex.day_mark) {
            marker.x = bar_box.x;
            marker.y = 0;
            marker.w = bar_box.x;
            marker.h = dims.h;
            host_api->AaediHAM_GraphicsDrawLine(aaediclock_Color{64, 64, 128, 64}, marker);
      }
      bar_box.x += bar_box.w;
    }
    // draw Yaxis markers

    marker.x = 0;
    marker.w = dims.w;
    for (int c = 0; c < 10 ; c++) {
        float y = (dims.h/4.0f)+((dims.h*.75f)/10.0f)*c;
        marker.y = y;
        marker.h = y;
        host_api->AaediHAM_GraphicsDrawLine(aaediclock_Color{64, 64, 128, 128}, marker);
    }



//=================================================================================

    aaediclock_Color tempcolor={128,128,128,0};
    bar_box.x= 2;
    bar_box.y=(dims.h/4)+ 2;
    bar_box.w=(dims.w/4)*3;
    bar_box.h=dims.h/16;
    char tempfloat[255];
    if (!speed_prime.empty() && !density_prime.empty()) {
        sprintf (tempfloat, "K Index: %.1f S': %.1f D': %.1f", klast, speed_prime.back(), density_prime.back());
    } else {
        sprintf (tempfloat, "K Index: %.1f S': BAD WIND", klast);
    }
    host_api->AaediHAM_GraphicsDrawText(tempfloat, tempcolor, bar_box);
//    panel.render_text(bar_box, Sans, tempcolor, tempfloat);
    return;


}

const char* kindex_plugin::getName() const {
    return "Kindex Module";
}

void kindex_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

