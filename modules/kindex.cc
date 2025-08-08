#include "../aaediclock.h"
#include "../utils.h"
#include "kindex.h"
#include <deque>
#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#endif

struct KIndexPoint {
    float kindex;
    bool day_mark;
    time_t timestamp;
};

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

json merge_json (const char* k_index_list, const char* solar_wind_list) {
   json merged = json::object();
   json k_list = json::parse(k_index_list);
   json solar_index = json::parse(solar_wind_list);
   merged["kindex"] = json::array();
   for (auto spot : k_list) {
     if (!spot.is_array() || spot.size() < 4) continue;
       std::string index_string;
       try {
         json new_node = json::object();
         new_node["time_tag"] = spot[0].get<std::string>();
         new_node["day_boundary"] = false;
         std::string time_tag = new_node["time_tag"].get<std::string>();
         size_t space_pos = time_tag.find(' ');
         if (space_pos != std::string::npos && time_tag.size() > space_pos + 5) {
           std::string time_part = time_tag.substr(space_pos + 1, 5);
           if (time_part == "00:00") {
             new_node["day_boundary"] = true;
           }
         }
         new_node["timestamp"] = parse_time_tag(time_tag);
         index_string = spot[1].get<std::string>();
         new_node["Kp"] = std::stof(index_string);
         index_string = spot[2].get<std::string>();
         new_node["a_running"] = std::stoi(index_string);
         index_string = spot[3].get<std::string>();
         new_node["station_count"] = std::stoi(index_string);
         merged["kindex"].push_back(new_node);
       } catch (const std::exception& e) {
//     	 SDL_Log("Failed to parse entry: %s -- %s", index_string.c_str(), e.what());
       }
     }
//   SDL_Log ("Processed K-indices");

     merged["solar_wind"]= json::array();
     for (auto spot : solar_index) {
       std::string index_string;
       try {
         if (!spot.is_array() || spot.size() < 4) continue;
           json new_node = json::object();
           new_node["time_tag"] = spot[0].get<std::string>();
           std::string time_tag = new_node["time_tag"].get<std::string>();
           new_node["day_boundary"] = false;
           size_t space_pos = time_tag.find(' ');
           if (space_pos != std::string::npos && time_tag.size() > space_pos + 5) {
             std::string time_part = time_tag.substr(space_pos + 1, 5);
             if (time_part == "00:00") {
               new_node["day_boundary"] = true;
             }
         }
         new_node["timestamp"] = parse_time_tag(time_tag);
         index_string = spot[1].get<std::string>();
         new_node["density"] = std::stof(index_string);
         index_string = spot[2].get<std::string>();
         new_node["speed"] = std::stof(index_string);
         index_string = spot[3].get<std::string>();
         new_node["temperature"] = std::stoi(index_string);
         merged["solar_wind"].push_back(new_node);
       } catch (const std::exception& e) {
//     	 SDL_Log("Failed to parse entry: %s -- %s", index_string.c_str(), e.what());
       }
     }
     return merged;
}



void k_index_chart (ScreenFrame& panel) {
    std::vector<struct KIndexPoint>k_indices;
    json merged;
    char* k_index_list = 0 ;
    char* solar_wind_list = 0;
    json source_json;
    Uint32 data_size;
    time_t cache_time;
    bool reload_flag = false;
    std::string combined;
    SDL_Log ("Kindex checking cache");
    data_size = cache_loader(MOD_KINDEX, (void**)&k_index_list, &cache_time);
    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
    } else {
        try {
            merged = json::parse(k_index_list);
        } catch (const json::parse_error &e) {
            SDL_Log("Kindex Cache Json Parse Error %u bytes %s\n", data_size, k_index_list);
            reload_flag=true;
        }
    }

    if (reload_flag) {
        SDL_Log("Kindex cache Miss");
        data_size = http_loader("https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json", (void**)&k_index_list);   // live
        data_size += http_loader("https://services.swpc.noaa.gov/products/solar-wind/plasma-7-day.json", (void**)&solar_wind_list);
        SDL_Log("Fetched Sources");
        if (data_size) {
            json k_list = json::parse(k_index_list);
            json solar_index = json::parse(solar_wind_list);
            merged = merge_json(k_index_list, solar_wind_list);


//            SDL_Log("Merged Sources");
            std::string combined =  merged.dump(5);
            add_data_cache(MOD_KINDEX, combined.length(), combined.c_str());
        }
//        SDL_Log("Cached Sources");
    }

    int goodread;
    goodread = 1;

    // clear the box
    panel.Clear();
    SDL_FRect bar_box;
//    SDL_Log ("Rendering K index graph");
        std::vector<float>speed_prime;
        std::vector<float>density_prime;

    if (goodread) {

        for (auto spot : merged["kindex"]) {
            if (!spot.contains("Kp") || !spot.contains("time_tag")) continue;
//            std::string index_string;
            try {
//                float temp = spot["Kp"];
//                bool day_boundary = spot["day_boundary"].get<bool>();
//                std::string time_tag = spot["time_tag"].get<std::string>();
//                SDL_Log ("See entry %f", temp);
                k_indices.push_back({spot["Kp"], spot["day_boundary"].get<bool>(), spot["timestamp"].get<time_t>()});
                } catch (const std::exception& e) {
//                    SDL_Log("Failed to parse entry: %s -- %s", index_string.c_str(), e.what());
                }
        }
        SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);


        bar_box.x=1;
        bar_box.w = (panel.dims.w-2)/k_indices.size();
        SDL_Log("Drawing chart size %zu", k_indices.size());
        auto wind_index =  merged["solar_wind"].begin();
        std::deque<float>speed_queue;
        std::deque<float>density_queue;

        // render data
        for (auto spot : k_indices ) {
            speed_prime.clear();
            density_prime.clear();
            if (spot.kindex < 1) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 128, 0, 255);
            } else if (spot.kindex <2) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 128, 128, 255);
            } else if (spot.kindex <3) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 0, 128, 255);
            }
             else if (spot.kindex <4) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 0, 255, 255);
            }
            else if (spot.kindex <5) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 255, 255, 255);
            }
            else if (spot.kindex <6) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 128, 0, 255);
            }
            else if (spot.kindex <7) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 0, 0, 255);
            }
            else if (spot.kindex <8) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 200, 0, 0, 255);
            } else {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 255, 0, 0, 255);
            }


            bar_box.h=((panel.dims.h*.75)/10)*spot.kindex;
            bar_box.y = panel.dims.h - bar_box.h;
            SDL_RenderFillRect(panel.GetRenderer(), &bar_box );


            float speed_old, density_old;
            speed_old = (*wind_index)["speed"].get<float>();
            density_old = (*wind_index)["density"].get<float>();

            while ((*wind_index)["timestamp"] < spot.timestamp) {
              wind_index++;
              speed_queue.push_back(speed_old - (*wind_index)["speed"].get<float>());
              density_queue.push_back(density_old - (*wind_index)["density"].get<float>());
              if (speed_queue.size() > 5) {
                speed_queue.pop_front();
              }
              speed_prime.push_back(std::accumulate(speed_queue.begin(), speed_queue.end(), 0.0f)/speed_queue.size());
              if (density_queue.size() > 5) {
                density_queue.pop_front();
              }
              density_prime.push_back(std::accumulate(density_queue.begin(), density_queue.end(), 0.0f)/density_queue.size());
//              speed_prime.push_back(speed_old - (*wind_index)["speed"].get<float>());
//              density_prime.push_back(density_old - (*wind_index)["density"].get<float>());
              speed_old = (*wind_index)["speed"].get<float>();
              density_old = (*wind_index)["density"].get<float>();
            }
            if (speed_prime.size()) {
              std::vector<SDL_FPoint> chart_pts;
              float point_width = bar_box.w/speed_prime.size();
              SDL_FPoint new_point;
              new_point.x = bar_box.x;
              for (auto solar_speed : speed_prime) {
//                if (solar_speed <0) {
//                  solar_speed *= -1;
//                }

//                new_point.y = panel.dims.h - ((solar_speed/100)*panel.dims.h);
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
//                if (solar_density <0) {
//                  solar_density *= -1;
//                }
                new_point.y = (panel.dims.h/8)+ ((solar_density/100)*(panel.dims.h/8));
                new_point.y -= (panel.dims.h/8)*7;
                chart_pts.push_back(new_point);
                new_point.x += point_width;
              }
              SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 255, 64, 200);
              SDL_RenderLines(panel.GetRenderer(), chart_pts.data(), chart_pts.size());
            }


            if (spot.day_mark) {
                SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 64, 128, 64);
                SDL_RenderLine(panel.GetRenderer(), bar_box.x,0, bar_box.x, panel.dims.h);
            }
            bar_box.x += bar_box.w;
        }

    } else {
        SDL_Log ("BAD READ");
    }




        // draw chart lines
        SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 64, 128, 128);
        for (int c = 0; c < 10 ; c++) {
            int y = (panel.dims.h/4)+((panel.dims.h*.75)/10)*c;
            SDL_RenderLine(panel.GetRenderer(), 0,y, panel.dims.w, y);
        }
        if (k_indices.size() >0) {
            SDL_Color tempcolor={128,128,128,0};
            bar_box.x= 2;
            bar_box.y=(panel.dims.h/4)+ 2;
            bar_box.w=(panel.dims.w/4)*3;
            bar_box.h=panel.dims.h/16;
            char tempfloat[255];
            sprintf (tempfloat, "K Index: %.1f S': %.1f D': %.1f", k_indices.back().kindex, speed_prime.back(), density_prime.back());
            panel.render_text(bar_box, Sans, tempcolor, tempfloat);
        }

//    SDL_Log ("Read Sat List");

    return;


}
