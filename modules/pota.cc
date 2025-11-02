#include "pota.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
SDL_TimerID pota_timer = 0;


struct pota_spot {
    char activator[32];
    char mode[16];
    char park[16];
    double	latitude;
    double	longitude;
    double      frequency;
};


std::string pota_json_parser(const char* input_string) {

    std::ostringstream cache_stream;
    int goodread = 1;
    json spot_list;
    try {
        spot_list=json::parse(input_string);
    } catch (const json::parse_error &e) {
        debug_log << "POTA: Json Parse Error " << strlen(input_string) << " bytes " << input_string << "\n";
        return "";
    }

    for (auto spot : spot_list) {
        if (spot.contains("latitude") &&
                spot["latitude"].is_number() &&
                spot.contains("longitude") &&
                spot["longitude"].is_number() &&
                spot.contains("activator") &&
                spot.contains("frequency") &&
                spot.contains("reference") &&
                spot.contains("mode") ) {

            struct pota_spot new_cache;
            memset(&new_cache, 0, sizeof(new_cache));
            std::string instring;
            instring 		= spot["activator"].template get<std::string>();
            strncpy(new_cache.activator, instring.c_str(), 31);
            instring		= spot["mode"].template get<std::string>();
            strncpy(new_cache.mode, instring.c_str(), 15);
            instring		= spot["reference"].template get<std::string>();
            strncpy(new_cache.park, instring.c_str(), 15);
            new_cache.latitude  = spot["latitude"].template get<double>();
            new_cache.longitude = spot["longitude"].template get<double>();
            instring            = spot["frequency"].template get<std::string>();
            new_cache.frequency = stod(instring)/1000;
            cache_stream.write(reinterpret_cast<const char*>(&new_cache), sizeof(new_cache));
        }
    }
    return (cache_stream.str());
}
void fetch_pota () {
     char* json_spots = 0 ;
     Uint32 data_size = 0;
     debug_log <<"POTA: Fetching Spots from pota.app via timer\n";
     SDL_Log("Fetching Spots from pota.app via timer");
     data_size = http_loader("https://api.pota.app/spot/activator", (void**)&json_spots);                           // live

     if (data_size) {
          std::string blob = pota_json_parser(json_spots);
          add_data_cache(MOD_POTA, blob.length(), (void*)blob.data());
          SDL_Log("Stored POTA");
          if(json_spots) {
               free (json_spots);
               json_spots=0;
          }
     }
     return;
}

Uint32 SDLCALL fetch_pota (void *userdata, SDL_TimerID timerID, Uint32 interval) {
     if (timerID) {
          fetch_pota();
          return (300000);
     } else {
          return 0;
     }
}



int pota_page[2]={0,2};
void pota_spots(ScreenFrame& panel, TTF_Font* font) {
     if (!pota_timer) {
//          fetch_pota();
          pota_timer = SDL_AddTimer(30, fetch_pota, NULL);
     }

     if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
         SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
     }
     else {
         SDL_Log("POTA DRAW during resize event!");
         return;
     }
    char* json_spots = 0 ;

    int c, tot;
    c=0;
    tot=0;

    int pin_alpha = 192;
    char tempstr[64];
    SDL_FRect TextRect;

    SDL_Color pota_color;
    pota_color.r = 0;
    pota_color.g = 128;
    pota_color.b = 0;
    pota_color.a = 0;
    Uint32 data_size;
    time_t cache_time;
    int reload_flag =0;
    std::istringstream spots_raw;
    // fetch the POTA spot data
    delete_owner_pins(MOD_POTA);
    data_size = cache_loader(MOD_POTA, (void**)&json_spots, &cache_time);
    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 400) {
        reload_flag=1;
        if(json_spots) {
              free (json_spots);
              json_spots=0;
        }
    }
    debug_log << "POTA: READ "<< data_size << " FROM CACHE!!!!\n";
    if (reload_flag) {
  /*       debug_log <<"POTA: Fetching Spots from pota.app\n";
         SDL_Log("POTA: Fetching Spots from pota.app");
         data_size = http_loader("https://api.pota.app/spot/activator", (void**)&json_spots);                           // live
         if (data_size) {
             std::string blob = pota_json_parser(json_spots);
             add_data_cache(MOD_POTA, blob.length(), (void*)blob.data());
             data_size = blob.length();
             spots_raw.clear();
             spots_raw.str(blob);
             if(json_spots) {
              free (json_spots);
              json_spots=0;
             }
         }*/
    } else {
        spots_raw.clear();
        std::string sanitized(json_spots, data_size);
        spots_raw.str(sanitized);
             if(json_spots) {
              free (json_spots);
              json_spots=0;
             }
             debug_log << "POTA: from cache "<< spots_raw.str().size()<<" buffer size\n";
    }


    // convert the POTA JSON to an object
    int goodread;
    goodread = 1;

    // clear the box
    panel.Clear();

    // render the header
    TextRect.w=panel.dims.w/2-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    panel.render_text(TextRect, font, pota_color, "POTA ACTIVATORS");
    if (spots_raw.str().size() < 5) {
     goodread = false;
    }
    // set up for rendering the lista and submitting the pins
    TextRect.w=(panel.dims.w/4)-(panel.dims.w/20);
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=((panel.dims.h/11)+(panel.dims.h/150));;
    if (goodread) {
        struct pota_spot spot;
        while (spots_raw.read(reinterpret_cast<char*>(&spot), sizeof(spot))) {

             tot++;
             struct map_pin pota_pin;
             pota_pin.owner  =               MOD_POTA;
             sprintf(pota_pin.label, "%s", spot.activator);
             pota_pin.lat    =               spot.latitude;
             pota_pin.lon    =            spot.longitude;
             pota_pin.icon   =               0;
             pota_color.a = pin_alpha;
             pota_pin.color  =               pota_color;
             pota_color.a = 0;
             pota_pin.tooltip[0]=            0;
             add_pin(&pota_pin);
             if ((c >= pota_page[0]*9) && (c<(pota_page[0]*9)+9)) {
                 panel.render_text(TextRect, font, pota_color, pota_pin.label);
                 TextRect.x += (panel.dims.w/4)+2;
                 sprintf(tempstr, "%4.3f", (spot.frequency));
                 panel.render_text(TextRect, font, pota_color, tempstr);
                 TextRect.x += (panel.dims.w/4)+2;
                 if (spot.mode[0]) {
                  panel.render_text(TextRect, font, pota_color, spot.mode);
                 }
                 TextRect.x += (panel.dims.w/4);
                 if (spot.park[0]) {
                  panel.render_text(TextRect, font, pota_color, spot.park);
                 }
                 TextRect.x = 5;
                 TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
             }
             c++;
        }

        pota_page[0]++;
        pota_page[1]=0;
        if (pota_page[0] > (tot/9)) {
            pota_page[0]=0;
        }
        // render the total count of POTA activators
        TextRect.w=panel.dims.w/2-10;
        TextRect.h=panel.dims.h/11;
        TextRect.x=5+(panel.dims.w/2);
        TextRect.y=2;
        sprintf(tempstr, "%i", tot);
        panel.render_text(TextRect, font, pota_color, tempstr);
    } else {// good read
        TextRect.w=panel.dims.w-10;
        if (TextRect.w > 2) {
           panel.render_text(TextRect, font, pota_color, "NO POTA DATA");
        }
    }
    return;
}
