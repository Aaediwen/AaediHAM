#include "../aaediclock.h"
#include "../utils.h"
#include "kindex.h"
#include <deque>
#include <sstream>
#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#endif

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

static nlohmann::json::iterator wind_index;
static nlohmann::json::iterator wind_end;

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

void write_wind_cache(std::ostringstream& cache_stream, time_t max_timestamp) {
	time_t current_timestamp =0;
	std::vector<struct SolarWindPoint>raw_points;
	raw_points.clear();
	while ((wind_index != wind_end) && (current_timestamp < max_timestamp)) {
	     struct SolarWindPoint new_node;
             std::string index_string;
             try {
                  index_string = (*wind_index)[0].get<std::string>();
                  debug_log << "KINDEX: Time String: " <<  index_string.c_str() << "\n";
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
                  debug_log << "KINDES: Density String: " << index_string.c_str() << "\n";
                  new_node.density = std::stof(index_string);
                  index_string = (*wind_index)[2].get<std::string>();
                  debug_log << "KINDEX: Speed String: " << index_string.c_str() << "\n";
                  new_node.speed = std::stof(index_string);
                  index_string = (*wind_index)[3].get<std::string>();
                  debug_log << "KINDEX: Temp String: " << index_string.c_str()<< "\n";
                  new_node.temperature = std::stoi(index_string);
                  raw_points.push_back(new_node);
             } catch (const std::exception& e) {
                  debug_log << "KINDEX: Skipped Wind: " << e.what() << "\n";
             }

             wind_index++;
	}

	size_t count = raw_points.size();
        cache_stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& wind_point : raw_points) {
          cache_stream.put(2);
          cache_stream.write (reinterpret_cast<const char*>(&wind_point), sizeof(wind_point));
        }

	raw_points.clear();
	return;
}

std::string merge_json (const char* k_index_list, const char* solar_wind_list) {
     std::ostringstream cache_stream;
     cache_stream.clear();
     json k_list, solar_index;
     std::vector<struct KIndexPoint>raw_points;
     try {
          k_list = json::parse(k_index_list);
          solar_index = json::parse(solar_wind_list);
     } catch (const std::exception& e) {
          debug_log << "KINDEX: JSON Parse error reading Solar Data\n";
          return "";
     }

     raw_points.clear();
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
               raw_points.push_back(new_node);
          } catch (const std::exception& e) {
               debug_log << "KINDEX: Skipped Kindex: " << e.what() << "\n";
          }
     }

     size_t count = raw_points.size();
     cache_stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
     for (const auto& kindex : raw_points) {
          cache_stream.put(1);
          cache_stream.write (reinterpret_cast<const char*>(&kindex), sizeof(kindex));
          write_wind_cache(cache_stream, kindex.timestamp);
     }
     raw_points.clear();

     return (cache_stream.str());
}


void k_index_chart (ScreenFrame& panel) {
    std::string merged;
    std::istringstream data;
    if (SDL_TryLockMutex(resize_mutex)) {
        SDL_UnlockMutex(resize_mutex);
    }
    else {
        SDL_Log("Kindex call during resize event!");
        return;
    }
    char* k_index_list = 0 ;
    char* solar_wind_list = 0;
    json source_json;
    Uint32 data_size;
    time_t cache_time;
    bool reload_flag = false;
    std::string combined;
    debug_log << "KINDEX: Kindex checking cache\n";
    data_size = cache_loader(MOD_KINDEX, (void**)&k_index_list, &cache_time);
    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
        if (k_index_list) {
          free (k_index_list);
          k_index_list = 0;
        }
    } else {
        debug_log << "KINDEX: Cache size: " << data_size << "\n";
        merged.assign(k_index_list, data_size);
        if (k_index_list) {
          free (k_index_list);
          k_index_list = 0;
        }
    }

    if (reload_flag) {
        SDL_Log ("Fetching Solar Weather from NOAA");
        debug_log << "KINDEX: Kindex cache Miss fetching data from NOAA\n";
        data_size = http_loader("https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json", (void**)&k_index_list);   // live
        data_size += http_loader("https://services.swpc.noaa.gov/products/solar-wind/plasma-7-day.json", (void**)&solar_wind_list);
        debug_log << "KINDEX: Fetched Sources\n";
        if (data_size) {
            merged = merge_json(k_index_list, solar_wind_list);
            add_data_cache(MOD_KINDEX, merged.length(), (void*)merged.data());
            if (k_index_list) {
              free (k_index_list);
              k_index_list = 0;
            }
            if (solar_wind_list) {
              free (solar_wind_list);
              solar_wind_list = 0;
            }
        }
    }
    data.clear();
    debug_log << "KINDEX: Cache Data size! " <<  merged.size() << "\n";
    data.str(merged);

    // clear the box
    panel.Clear();
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_FRect bar_box;
    debug_log << "KINDEX: Rendering K index graph\n";
    std::vector<float>speed_prime;
    std::vector<float>density_prime;
    std::deque<float>speed_queue;
    std::deque<float>density_queue;
    float speed_old = 0;
    float density_old = 0;

    float klast, dlast, slast;
    uint8_t type;
    size_t kindex_count;
    if (merged.length() < 5) {
      debug_log << "KINDEX: Missing Solar Data!\n";
      return;
    }

    data.read(reinterpret_cast<char*>(&kindex_count), sizeof(kindex_count));
    debug_log << "KINDEX: Drawing chart size " << kindex_count << "\n";
    bar_box.x=1;
    bar_box.w = (panel.dims.w-2)/kindex_count;
    for (int ki = 0 ; ki < kindex_count ; ki++) {	// read each K Index value
      // reset the primes of actual solar wind display values
      speed_prime.clear();
      density_prime.clear();
      // read the kindex
      data.read(reinterpret_cast<char*>(&type), sizeof(type));
      if (type != 1) {
        debug_log << "KINDEX: Kindex read error! Type not 1 on Kindex read, got " <<  type << "\n";
        break;
      }
      struct KIndexPoint kindex;
      data.read (reinterpret_cast<char*>(&kindex), sizeof(kindex));

      // render the K-index bar graph
      klast = kindex.kindex;
      if (kindex.kindex < 1) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 128, 0, 255);
      } else if (kindex.kindex <2) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 128, 128, 255);
      } else if (kindex.kindex <3) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 0, 128, 255);
      }
       else if (kindex.kindex <4) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 0, 255, 255);
      }
      else if (kindex.kindex <5) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 255, 255, 255);
      }
      else if (kindex.kindex <6) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 128, 0, 255);
      }
      else if (kindex.kindex <7) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 0, 0, 255);
      }
      else if (kindex.kindex <8) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 200, 0, 0, 255);
      } else {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 255, 0, 0, 255);
      }
      bar_box.h=((panel.dims.h*.75)/10)*kindex.kindex;
      bar_box.y = panel.dims.h - bar_box.h;
      SDL_RenderFillRect(panel.GetRenderer(), &bar_box );



      // process solar wind info
      size_t wind_count;
      data.read(reinterpret_cast<char*>(&wind_count), sizeof(wind_count));
      for (int wi = 0 ; wi < wind_count ; wi++) {	// read each solar_wind value
        data.read(reinterpret_cast<char*>(&type), sizeof(type));
        if (type != 2) {
          debug_log << "KINDEX: Solar Wind read error! Type not 2 on Solar Wind read\n";
          break;
        }
        struct SolarWindPoint  wind_data;
        data.read (reinterpret_cast<char*>(&wind_data), sizeof(wind_data));

        // populatethe queues and calc the primes
        speed_queue.push_back(speed_old - wind_data.speed);
        density_queue.push_back(density_old - wind_data.density);
        if (speed_queue.size() > 5) {
          speed_queue.pop_front();
        }
        speed_prime.push_back(std::accumulate(speed_queue.begin(), speed_queue.end(), 0.0f)/speed_queue.size());
        if (density_queue.size() > 5) {
          density_queue.pop_front();
        }
        density_prime.push_back(std::accumulate(density_queue.begin(), density_queue.end(), 0.0f)/density_queue.size());
        // update the 'previous' state
        speed_old = wind_data.speed;
        density_old = wind_data.density;
      }
      // plot the graphs for solar wind
      if (speed_prime.size()) {
        std::vector<SDL_FPoint> chart_pts;
        float point_width = bar_box.w/speed_prime.size();
        SDL_FPoint new_point;
        new_point.x = bar_box.x;
        for (auto solar_speed : speed_prime) {
          new_point.y = (panel.dims.h/8)+((solar_speed/100)*(panel.dims.h/8));
          chart_pts.push_back(new_point);
          new_point.x += point_width;
        }
        SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 64, 64, 64);
        SDL_RenderLines(panel.GetRenderer(), chart_pts.data(), chart_pts.size());
      }

      if (density_prime.size()) {
        std::vector<SDL_FPoint> chart_pts;
        float point_width = bar_box.w/density_prime.size();
        SDL_FPoint new_point;
        new_point.x = bar_box.x;
        for (auto solar_density : density_prime) {
          new_point.y = (panel.dims.h/8)+ ((solar_density/100)*(panel.dims.h/8));
          new_point.y -= (panel.dims.h/8)*7;
          chart_pts.push_back(new_point);
          new_point.x += point_width;
        }
        SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 255, 64, 200);
        SDL_RenderLines(panel.GetRenderer(), chart_pts.data(), chart_pts.size());
      }


      // -------- end processing wind info

      // prep for the next kindex

      if (kindex.day_mark) {
          SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 64, 128, 64);
          SDL_RenderLine(panel.GetRenderer(), bar_box.x,0, bar_box.x, panel.dims.h);
      }
      bar_box.x += bar_box.w;
    }
    // draw Yaxis markers
    SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 64, 128, 128);
    for (int c = 0; c < 10 ; c++) {
        int y = (panel.dims.h/4)+((panel.dims.h*.75)/10)*c;
        SDL_RenderLine(panel.GetRenderer(), 0,y, panel.dims.w, y);
    }



//=================================================================================

    SDL_Color tempcolor={128,128,128,0};
    bar_box.x= 2;
    bar_box.y=(panel.dims.h/4)+ 2;
    bar_box.w=(panel.dims.w/4)*3;
    bar_box.h=panel.dims.h/16;
    char tempfloat[255];
    sprintf (tempfloat, "K Index: %.1f S': %.1f D': %.1f", klast, speed_prime.back(), density_prime.back());
    panel.render_text(bar_box, Sans, tempcolor, tempfloat);
    return;
}
