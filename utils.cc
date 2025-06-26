#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"

void draw_panel_border(struct ScreenFrame panel) {
    SDL_SetRenderDrawColor(surface, 128, 128, 128, 255);
    SDL_FRect border;
    border.x=0;
    border.y=0;
    border.w=panel.dims.w;
    border.h=panel.dims.h;
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderRect(surface, &(border));
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    return;
}

double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg) {
    //Converts latitude and solar declination from degrees to radians
    double lat = lat_deg * M_PI / 180.0;
    double decl = decl_deg * M_PI / 180.0;

    double utc_hours = utc->tm_hour + utc->tm_min / 60.0 + utc->tm_sec / 3600.0;
    double solar_time = utc_hours + (lon_deg / 15.0);  // Local solar time for pixel
    double hour_angle = (15.0 * (solar_time - 12.0)) * M_PI / 180.0;
    double sin_alt = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hour_angle);
    return asin(sin_alt) * 180.0 / M_PI;
}

void maidenhead(double lat, double lon, char* maiden) {

    // generate maidenhead grid square
    // result should be at least 7 bytes long

    char latstr[2];
    char lonstr[2];
    double madlon, madlat;
    madlon = lon + 180;
    madlat = lat + 90;
    maiden[6]=0;
    maiden[0]=((int)(madlon/20))+65;	// Offset from 'A'
    maiden[1]=((int)(madlat/10))+65;
    maiden[2]=(int)(((int)madlon % 20)/2)+48;	// offset from '0'
    maiden[3]=(int)(((int)madlat + 90) % 10)+48;
    maiden[4] = (int)(((fmod(madlon,2.0))/2.0)*24)+97;	// offset from 'a'
    maiden[5] = (int)((fmod(madlat,1.0))*24)+97;
    return;
}

void sun_times(double lat, double lon, time_t* sunrise, time_t* sunset, double *solar_alt, time_t now) {
    // fet sunrise and sunset times
    tm* utc = gmtime(&now);
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
    *solar_alt = solar_altitude(lat, lon, utc, solar_decl);
    // find the next zero crossing for sunrise if current alt <0

    double test_alt;
    test_alt = *solar_alt;
    tm* test_time;
    *sunrise = now;
    *sunset = now;
    if (test_alt <0) {
        while (test_alt <0) {
            *sunrise +=5;
            *sunset += 5;
            test_time = gmtime(sunrise);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt >0) {
            *sunset += 5;
            test_time = gmtime(sunset);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
    } else {
       while (test_alt >0) {
            *sunrise +=5;
            *sunset += 5;
            test_time = gmtime(sunset);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt <0) {
            *sunrise += 5;
            test_time = gmtime(sunrise);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
    }

}

void render_text(SDL_Renderer *renderer, SDL_Texture *target, SDL_FRect * text_box, TTF_Font *font, SDL_Color color, const char* str) {

    SDL_Surface* textsurface;
    SDL_Texture *TextTexture;

    // render a text string
//    textsurface = TTF_RenderText_Shaded(font, str, strlen(str), color, color);
    SDL_Color bg_color = {0, 0, 0,255};
    textsurface = TTF_RenderText_LCD(font, str, strlen(str), color, bg_color);
    if (textsurface==NULL) {
        SDL_Log("Text render error: %s", SDL_GetError());
        return;
    }
    TextTexture = SDL_CreateTextureFromSurface(renderer, textsurface);
    SDL_SetRenderTarget(renderer, target);
    SDL_RenderTexture(renderer, TextTexture, NULL, text_box);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);
}

int add_pin(struct map_pin* new_pin) {
    // add a new map pin for a module
//    SDL_Log("Adding pin %s", new_pin->label);
    struct map_pin* empty_pin;
    struct map_pin* current_pin;
    if (map_pins) {
        current_pin = map_pins;
        while (current_pin->next) {
            current_pin = current_pin->next;
        }
        current_pin->next = (struct map_pin*)malloc(sizeof(struct map_pin));
        empty_pin = current_pin->next;
    } else {
        map_pins = (struct map_pin*)malloc(sizeof(struct map_pin));
        empty_pin=map_pins;
    }
    empty_pin->next=0;
    empty_pin->owner = new_pin->owner;
    empty_pin->lat = new_pin->lat;
    empty_pin->lon = new_pin->lon;
    empty_pin->icon = new_pin->icon;
    empty_pin->color = new_pin->color;
    empty_pin->label[0]=0;
    if (new_pin->label[0]) {
        memcpy(empty_pin->label, new_pin->label, 16);
        empty_pin->label[15]=0;
    }
    empty_pin->tooltip[0]=0;
    if (new_pin->tooltip[0]) {
        memcpy(empty_pin->tooltip, new_pin->tooltip, 512);
        empty_pin->label[511]=0;
    }
    return (0);
}

int delete_owner_pins(enum mod_name owner) {
    // delete all map pins owned by a module
//    SDL_Log("killing pins %i", owner);
    struct map_pin* current_pin;
    struct map_pin* next_pin;
    struct map_pin* last_pin;
    struct map_pin* old_pin;
    if (map_pins) {
        current_pin=map_pins;
//        SDL_Log("Initial map_pins: %p", map_pins);
        last_pin=0;
        while (current_pin) {
//            SDL_Log("Current pin: %p, owner: %d", current_pin, current_pin->owner);
            if (!current_pin) {
                SDL_Log("Null current_pin!");
                break;
            }

            next_pin = current_pin->next;
//            SDL_Log("Next pin: %p", next_pin);
            if (current_pin->owner == owner) {
//                SDL_Log("Removing pin");
                // delete current pin
                old_pin = current_pin;
                if (current_pin == map_pins) {
                    map_pins = next_pin;
                }
                if (last_pin) {
                    last_pin->next = next_pin;
                }
                current_pin = next_pin;
                free (old_pin);
            } else {
//                SDL_Log("NOT Removing pin");
                last_pin = current_pin;
                current_pin=next_pin;
            }
        }
    }
    return(0);
}


int delete_mod_cache(enum mod_name owner) {
//    SDL_Log("killing pins %i", owner);
    struct data_blob* current_chunk;
    struct data_blob* next_chunk;
    struct data_blob* last_chunk;
    struct data_blob* old_chunk;
    if (data_cache) {
        current_chunk=data_cache;
        last_chunk=0;
        while (current_chunk) {
            if (!current_chunk) {
                SDL_Log("Null current chunk!");
                break;
            }

            next_chunk = current_chunk->next;
            if (current_chunk->owner == owner) {
                // delete current pin
                old_chunk = current_chunk;
                if (current_chunk == data_cache) {
                    data_cache = next_chunk;
                }
                if (last_chunk) {
                    last_chunk->next = next_chunk;
                }
                current_chunk = next_chunk;
                free (old_chunk->data);
                free (old_chunk);
            } else {
                last_chunk = current_chunk;
                current_chunk=next_chunk;
            }
        }
    }
    return(0);

}

int add_data_cache(enum mod_name owner, const Uint32 size, void* data) {
    delete_mod_cache(owner);

    // add a new data_cache for a module
    struct data_blob* empty_locker;
    struct data_blob* current_chunk;
    if (data_cache) {
        current_chunk = data_cache;
        while (current_chunk->next) {
            current_chunk = current_chunk->next;
        }
        current_chunk->next = (struct data_blob*)malloc(sizeof(struct data_blob));
        empty_locker = current_chunk->next;
    } else {
        data_cache = (struct data_blob*)malloc(sizeof(struct data_blob));
        empty_locker=data_cache;
    }
    empty_locker->next=0;
    empty_locker->owner = owner;
    empty_locker->fetch_time=time(NULL);
    empty_locker->size = size;
    empty_locker->data = malloc(size+1);
    if (empty_locker->data) {
        memcpy(empty_locker->data, data, size);
    } else {
        return (0);
    }

//    printf ("Test stored data\n %s \n -----------\n",(char*)empty_locker->data);
    return (1);

}

int fetch_data_cache(enum mod_name owner, time_t *age, Uint32 *size, void* data) {
    struct data_blob* current = 0;
    if (data_cache) {
        struct data_blob* current = data_cache;
        while (current) {
            if (current->owner == owner) {
//                printf ("Test fetched data\n %s \n -----------\n",(char*)current->data);
                memcpy(age, &(current->fetch_time), sizeof(time_t));
                memcpy(size, &(current->size), sizeof(Uint32));
                if (data != NULL) {
                    memset(data, 0, current->size + 1);
                    memcpy(data, current->data, current->size);
//                    SDL_Log("Cache returning %i bytes", current->size);
                }
                return (1);
            }
            current = current->next;
        }
    }
    return (0);
}