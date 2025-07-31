#include "pota.h"
#include "../aaediclock.h"
#include "../utils.h"
#include "../json.hpp"
using json = nlohmann::json;



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
    // fetch the POTA spot data
    delete_owner_pins(MOD_POTA);
    data_size = cache_loader(MOD_POTA, (void**)&json_spots, &cache_time);
    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 300) {
        reload_flag=1;
    }
    if (reload_flag) {
         data_size = http_loader("https://api.pota.app/spot/activator", (void**)&json_spots);                           // live
//         data_size = http_loader("https://aaediwen.theaudioauthority.net/morse/activator", &json_spots);      // debug
         if (data_size) {
             add_data_cache(MOD_POTA, data_size, json_spots);
         }
    }


//    json_spots = *cache_data_address;
//    SDL_Log("POTA Cache loader call complete");
    // convert the POTA JSON to an object
    int goodread;
    goodread = 1;
    json spot_list;
    if (data_size && json_spots) {
//        SDL_Log("WE have SPOT data: %li bytes", strlen(json_spots));
        try {
            spot_list=json::parse(json_spots);
        } catch (const json::parse_error &e) {
            SDL_Log("POTA Json Parse Error %zu bytes %s\n", strlen(json_spots), json_spots);
            goodread=0;
        }
        free(json_spots);
        json_spots = nullptr;
    } else {
        SDL_Log("POTA Json FETCH Error");
        goodread=0;
    }
//    SDL_Log("Parsed JSON Data");
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
        for (auto spot : spot_list) {
            if (spot.contains("latitude") &&
                spot["latitude"].is_number() &&
                spot.contains("longitude") &&
                spot["longitude"].is_number() &&
                spot.contains("activator") &&
                spot.contains("frequency") &&
                spot.contains("reference") &&
                spot.contains("mode") ) {

                tot++;
                // submit the map pin
//                SDL_Log("adding pin");
                struct map_pin pota_pin;

                pota_pin.owner  =               MOD_POTA;
                std::string tempstdstring       =               spot["activator"].template get<std::string>();
                sprintf(pota_pin.label, "%s", tempstdstring.c_str());

                pota_pin.lat    =               spot["latitude"].template get<double>();
                pota_pin.lon    =               spot["longitude"].template get<double>();
                pota_pin.icon   =               0;
                pota_pin.color  =               pota_color;
                pota_pin.tooltip[0]=            0;
                add_pin(&pota_pin);
//                SDL_Log("pin added");
                // add to screen list
                if ((c >= pota_page[0]*9) && (c<(pota_page[0]*9)+9)) {
//                    SDL_Log("adding list");
                    std::string mode = spot["mode"].template get<std::string>();
                    std::string strfreq = spot["frequency"].template get<std::string>();
                    double freq = stod(strfreq)/1000;
                    std::string park = spot["reference"].template get<std::string>();


                    pota_color.a = 0;
                    panel.render_text(TextRect, font, pota_color, pota_pin.label);
                    TextRect.x += (panel.dims.w/4)+2;
                    sprintf(tempstr, "%4.3f", (freq));
                    panel.render_text(TextRect, font, pota_color, tempstr);
                    TextRect.x += (panel.dims.w/4)+2;
                    if (mode.size() >0) {
                        panel.render_text(TextRect, font, pota_color, mode.c_str());
                    }
                    TextRect.x += (panel.dims.w/4);
                    panel.render_text(TextRect, font, pota_color, park.c_str());
                    TextRect.x = 5;
                    TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
                    pota_color.a = 200;
//                    SDL_Log("added list");
                } // add to list?
                c++;
            } // spot valid?
        } // foreach spot
//        pota_page[1]++;
//        if (pota_page[1] > 5) {
            pota_page[0]++;
            pota_page[1]=0;
//        }
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
