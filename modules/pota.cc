#include "pota.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;


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
        SDL_Log("POTA Json Parse Error %zu bytes %s\n", strlen(input_string), input_string);
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

int pota_page[2]={0,2};
void pota_spots(ScreenFrame& panel, TTF_Font* font) {
//    SDL_Log("Drawing POTA");
    char* json_spots = 0 ;
//    char** cache_data_address;

    int c, tot;
    c=0;
    tot=0;


    char tempstr[64];
    SDL_FRect TextRect;

    SDL_Color pota_color;
    pota_color.r = 0;
    pota_color.g = 128;
    pota_color.b = 0;
    pota_color.a = 200;
    Uint32 data_size;
    time_t cache_time;
    int reload_flag =0;
    std::istringstream spots_raw;
    // fetch the POTA spot data
    delete_owner_pins(MOD_POTA);
    data_size = cache_loader(MOD_POTA, (void**)&json_spots, &cache_time);
    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 300) {
        reload_flag=1;
    }
//    SDL_Log ("READ %i FROM CACHE!!!!", data_size);
    if (reload_flag) {
         data_size = http_loader("https://api.pota.app/spot/activator", (void**)&json_spots);                           // live
//         data_size = http_loader("https://aaediwen.theaudioauthority.net/morse/activator", &json_spots);      // debug
         if (data_size) {
             std::string blob = pota_json_parser(json_spots);
//             SDL_Log("POTA STRING: %zu size", blob.length());
             add_data_cache(MOD_POTA, blob.length(), (void*)blob.data());
             data_size = blob.length();
             spots_raw.clear();
             spots_raw.str(blob);
         }
    } else {
        spots_raw.clear();
        std::string sanitized(json_spots, data_size);
        spots_raw.str(sanitized);
//        SDL_Log("from cache  %zu buffer size", spots_raw.str().size());
    }


//    json_spots = *cache_data_address;
//    SDL_Log("POTA Cache loader call complete");
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
    pota_color.a = 0;
    panel.render_text(TextRect, font, pota_color, "POTA ACTIVATORS");

    // set up for rendering the lista and submitting the pins
    pota_color.a = 200;
    TextRect.w=(panel.dims.w/4)-(panel.dims.w/20);
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=((panel.dims.h/11)+(panel.dims.h/150));;
//    SDL_Log("rendered header");
    if (goodread) {
        struct pota_spot spot;
//        SDL_Log("Reading from %zu buffer size", spots_raw.str().size());
        while (spots_raw.read(reinterpret_cast<char*>(&spot), sizeof(spot))) {

             tot++;
             struct map_pin pota_pin;
             pota_pin.owner  =               MOD_POTA;
             sprintf(pota_pin.label, "%s", spot.activator);
             pota_pin.lat    =               spot.latitude;
             pota_pin.lon    =            spot.longitude;
             pota_pin.icon   =               0;
             pota_pin.color  =               pota_color;
             pota_pin.tooltip[0]=            0;
             add_pin(&pota_pin);
             if ((c >= pota_page[0]*9) && (c<(pota_page[0]*9)+9)) {
                 pota_color.a = 0;
                 panel.render_text(TextRect, font, pota_color, pota_pin.label);
                 TextRect.x += (panel.dims.w/4)+2;
                 sprintf(tempstr, "%4.3f", (spot.frequency));
                 panel.render_text(TextRect, font, pota_color, tempstr);
                 TextRect.x += (panel.dims.w/4)+2;
                 panel.render_text(TextRect, font, pota_color, spot.mode);
                 TextRect.x += (panel.dims.w/4);
                 panel.render_text(TextRect, font, pota_color, spot.park);
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
//        SDL_Log("rendering count");
        // render the total count of POTA activators
        TextRect.w=panel.dims.w/2-10;
        TextRect.h=panel.dims.h/11;
        TextRect.x=5+(panel.dims.w/2);
        TextRect.y=2;
        pota_color.a = 0;
        sprintf(tempstr, "%i", tot);
        panel.render_text(TextRect, font, pota_color, tempstr);
//        SDL_Log("done rendering spots");
    } // good read
    // clean up
//    SDL_SetRenderTarget(surface, NULL);
//    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
}
