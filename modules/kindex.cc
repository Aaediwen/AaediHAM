#include "../aaediclock.h"
#include "../utils.h"
#include "kindex.h"

void k_index_chart (ScreenFrame& panel) {
    std::vector<float>k_indices;
    char* k_index_list = 0 ;
    Uint32 data_size;
    time_t cache_time;
    bool reload_flag = false;
    data_size = cache_loader(MOD_KINDEX, &k_index_list, &cache_time);
    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
    }

    if (reload_flag) {
        data_size = http_loader("https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json", &k_index_list);   // live
        if (data_size) {
            add_data_cache(MOD_KINDEX, data_size, k_index_list);
        }
    }

    int goodread;
    goodread = 1;
    json k_list;
    if (data_size && k_index_list) {
        try {
            k_list=json::parse(k_index_list);
        } catch (const json::parse_error &e) {
            SDL_Log("Kindex Json Parse Error %li bytes %s\n", strlen(k_index_list), k_index_list);
            goodread=0;
        }
        free(k_index_list);
        k_index_list = nullptr;
    } else {
        SDL_Log("K Index Json FETCH Error");
        goodread=0;
    }
    // clear the box
    panel.Clear();
    SDL_FRect bar_box;
//    SDL_Log ("Rendering K index graph");
    if (goodread) {

        for (auto spot : k_list) {
            if (!spot.is_array() || spot.size() < 2) continue;
            std::string index_string;
            try {
                index_string = spot[1].get<std::string>();
                float temp = std::stof(index_string);
                std::string time_tag = spot[0].get<std::string>();
//                SDL_Log ("See entry %f", temp);
                k_indices.push_back(temp);
                } catch (const std::exception& e) {
//                    SDL_Log("Failed to parse entry: %s -- %s", index_string.c_str(), e.what());
    }

        }
        SDL_SetRenderTarget(panel.renderer, panel.texture);


        bar_box.x=1;
        bar_box.w = (panel.dims.w-2)/k_indices.size();
//        SDL_Log("Drawing chart size %zu", k_indices.size());

        // render data
        for (auto spot : k_indices ) {
            if (spot < 1) {
                SDL_SetRenderDrawColor(panel.renderer, 0, 128, 0, 255);
            } else if (spot <2) {
                SDL_SetRenderDrawColor(panel.renderer, 0, 128, 128, 255);
            } else if (spot <3) {
                SDL_SetRenderDrawColor(panel.renderer, 0, 0, 128, 255);
            }
             else if (spot <4) {
                SDL_SetRenderDrawColor(panel.renderer, 0, 0, 255, 255);
            }
            else if (spot <5) {
                SDL_SetRenderDrawColor(panel.renderer, 0, 255, 255, 255);
            }
            else if (spot <6) {
                SDL_SetRenderDrawColor(panel.renderer, 128, 128, 0, 255);
            }
            else if (spot <7) {
                SDL_SetRenderDrawColor(panel.renderer, 128, 0, 0, 255);
            }
            else if (spot <8) {
                SDL_SetRenderDrawColor(panel.renderer, 200, 0, 0, 255);
            } else {
                SDL_SetRenderDrawColor(panel.renderer, 255, 0, 0, 255);
            }


            bar_box.h=(panel.dims.h/10)*spot;
            bar_box.y = panel.dims.h - bar_box.h;
            SDL_RenderFillRect(panel.renderer, &bar_box );
            bar_box.x += bar_box.w;
        }

    } else {
        SDL_Log ("BAD READ");
    }




        // graw chart lines
        SDL_SetRenderDrawColor(panel.renderer, 64, 64, 128, 128);
        for (int c = 0; c < 10 ; c++) {
            SDL_RenderLine(panel.renderer, 0,(panel.dims.h/10)*c, panel.dims.w, (panel.dims.h/10)*c);
        }
        if (k_indices.size() >0) {
            SDL_Color tempcolor={128,128,128,0};
            bar_box.x=2;
            bar_box.y=2;
            bar_box.w=panel.dims.w/2;
            bar_box.h=panel.dims.h/8;
            char tempfloat[15];
            sprintf (tempfloat, "K Index: %.1f", k_indices.back());
            panel.render_text(bar_box, Sans, tempcolor, tempfloat);
        }

//    SDL_Log ("Read Sat List");

    return;


}
