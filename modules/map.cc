#include "../aaediclock.h"
#include "../utils.h"

void load_maps(SDL_Renderer* surface) {
//    SDL_Log("Reloading Maps");
    DayMap.surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    if (DayMap.surface) {
        DayMap.texture = SDL_CreateTextureFromSurface(surface,DayMap.surface);
        if (!DayMap.texture) {
            SDL_Log("Unable to load DayMap Texture: %s\n", SDL_GetError());
            exit(1);
        }
    } else {
        SDL_Log("Unable to load DayMap Surface: %s\n", SDL_GetError());
        exit(1);
    }
    NightMap.surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    if (NightMap.surface) {
        NightMap.texture = SDL_CreateTextureFromSurface(surface,NightMap.surface);
        if (!NightMap.texture) {
        SDL_Log("Unable to load NightMap Texture: %s\n", SDL_GetError());
        }
    } else {
        SDL_Log("Unable to load NightMap Surface: %s\n", SDL_GetError());
        exit(1);
    }

    if ((!DayMap.texture) || (!NightMap.texture)) {
        SDL_Log("Unable to load Maps");
    }

    SDL_SetTextureBlendMode(DayMap.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(NightMap.texture, SDL_BLENDMODE_NONE);
    CountriesMap.surface = SDL_LoadBMP("images/outline.bmp");
    if (CountriesMap.surface) {
        int x, y;
        Uint8 cg, cr, cb;
        Uint8* country_pixels = (Uint8*)CountriesMap.surface->pixels;
        const Uint8 bpp = SDL_GetPixelFormatDetails(CountriesMap.surface->format)->bytes_per_pixel;

        for ( y = 0; y < CountriesMap.surface->h ; y++) {
            for (x=0 ; x < CountriesMap.surface->w ; x++) {
                int pixel_index = ( CountriesMap.surface->w * bpp * y ) + ( bpp * x );
                Uint32 *pixel_val=(Uint32*)(pixel_index+country_pixels);

                SDL_GetRGBA( *pixel_val, SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, &cr, &cg, &cb, NULL);
                Uint32 pixel_val_out = SDL_MapRGBA(SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, 0, 0, 0, (255-cg));
                memcpy((country_pixels + pixel_index), &pixel_val_out, bpp);
            }
        }
        CountriesMap.texture = SDL_CreateTextureFromSurface(surface,CountriesMap.surface);
        if (!CountriesMap.texture) {
                SDL_Log("Unable to load Country texture: %s\n", SDL_GetError());
                exit(1);
        }
    } else {
        SDL_Log("Unable to load Country Surface: %s\n", SDL_GetError());
        exit(1);
    }
//    SDL_Log("ALL MAPS LOADED %s\n", SDL_GetError());
    return;

}

void render_pin(ScreenFrame *panel, struct map_pin *current_pin) {

    SDL_Texture* icon_tex = SDL_CreateTexture(panel->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    if (!icon_tex) {
        SDL_Log("Failed to create icon texture: %s", SDL_GetError());
        return ;
    }
    // render the icon
     SDL_SetRenderTarget(panel->renderer, icon_tex);
    if (current_pin->icon) {
        SDL_RenderTexture(panel->renderer, current_pin->icon, NULL, NULL);
    } else {
         SDL_FRect pin_rect = {4.0f, 4.0f, 8.0f, 8.0f};
         SDL_SetRenderDrawColor(panel->renderer, 16, 16, 16, 128);
         SDL_RenderFillRect(panel->renderer, NULL);
         SDL_SetRenderDrawColor(panel->renderer, current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a);
         SDL_RenderFillRect(panel->renderer, &pin_rect );
    }
    SDL_FRect target_rect;
    target_rect.h=8;
    target_rect.w=8;
    SDL_FPoint tgt_px;
    cords_to_px(current_pin->lat, current_pin->lon, (int)panel->texture->w, (int)panel->texture->h, &tgt_px);
    target_rect.x=tgt_px.x;
    target_rect.x -= (target_rect.w/2);
    target_rect.y=tgt_px.y;
    target_rect.y -= (target_rect.h/2);
    if ((target_rect.x+ target_rect.w)  > panel->dims.w) {
        (target_rect.x -= target_rect.w);
    }
    if (target_rect.x <=0) {
        target_rect.x += target_rect.w;
    }
    SDL_SetTextureBlendMode(icon_tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(panel->renderer, panel->texture);
    SDL_RenderTexture(panel->renderer, icon_tex, NULL, &target_rect);
    SDL_SetRenderTarget(panel->renderer, NULL);
    SDL_DestroyTexture(icon_tex);
//    SDL_Log("DONE RENDERING PIN");
    return;
}

void regen_mask (SDL_Surface* source, SDL_Surface* dest, const SDL_FRect& panel_dims) {
    time_t nowtime = time(nullptr);
    struct tm utc;
//    tm utcroot; // check these on Linux
#ifdef _WIN32
    gmtime_s(&utc, &nowtime);
#else 
    gmtime_r(&nowtime, &utc);
#endif
    SDL_Rect panel_cords, source_cords;
    double softness = 10.0;
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc.tm_yday+1)) ));
//    SDL_Log("Regen Terminator Alpha Mask — source: %p, dest: %p, dims: %.1fx%.1f",
//    (void*)source, (void*)dest,
//    panel_dims.w, panel_dims.h);

    SDL_LockMutex(night_mask_mutex);    /// MUTEX LOCK

    Uint8* alpha_pixels = (Uint8*)dest->pixels;
    Uint8* source_pixels = (Uint8*)source->pixels;
    const SDL_PixelFormatDetails* source_details = SDL_GetPixelFormatDetails(source->format);
    const SDL_PixelFormatDetails* dest_details = SDL_GetPixelFormatDetails(dest->format);

    const Uint8 dest_bpp = dest_details->bytes_per_pixel;
    const Uint8 source_bpp = source_details->bytes_per_pixel;

    for (panel_cords.y=0 ; panel_cords.y < floor(panel_dims.h) ; panel_cords.y++) {
    //        SDL_Log("Calculating alpha row %i of %f", panel_cords.y, panel_dims.h);
            source_cords.y = (panel_cords.y/panel_dims.h)*source->h;
            double lat = 90.0 - (180.0 * panel_cords.y / (double)panel_dims.h);
            for (panel_cords.x=0 ; panel_cords.x < floor(panel_dims.w) ; panel_cords.x++) {
                Uint8 r, g, b;
                double lon = -180.0 + (360.0 * panel_cords.x / (double)panel_dims.w);
                double alt = solar_altitude(lat, lon, &utc, solar_decl);
                // calculate per pixel alpha
                Uint8 alpha;
                if (alt > softness) {
                    alpha = 255;
                } else if (alt < -softness) {
                    alpha = 0;
                } else {
                    alpha = (Uint8)(255.0 * (alt + softness) / (2.0 * softness));
                }
                // Write a pixel with the computed alpha

                source_cords.x = (panel_cords.x/panel_dims.w)*source->w;
                int source_pixel_index = ( source->w * source_bpp * source_cords.y ) + ( source_bpp * source_cords.x );
                int dest_pixel_index =   ( dest->w * dest_bpp * panel_cords.y ) + ( dest_bpp * panel_cords.x );
                Uint32 *source_pixel_val=(Uint32*)(source_pixel_index+source_pixels);
                SDL_GetRGBA( *source_pixel_val, source_details, NULL, &r, &g, &b, NULL);
                Uint32 dst_pixel_val = SDL_MapRGBA(dest_details, NULL, r, g, b, (255 - alpha));
                memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);

            }
        }

    SDL_UnlockMutex(night_mask_mutex);  /// MUTEX UNLOCK
    return;
}

Uint32 SDLCALL regen_mask (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    if (timerID) {
        struct regen_mask_args* args = (struct regen_mask_args*)userdata;
//        SDL_Log("Timer Callback — source: %p, dest: %p, dims: %.1fx%.1f",
//            (void*)args->source, (void*)args->dest,
//            args->panel_dims.w, args->panel_dims.h);
        regen_mask (args->source, args->dest, args->panel_dims);
        return (interval);
    } else {
        return 0;
    }
}

SDL_Surface* night_mask = nullptr;
SDL_Renderer* old_renderer = nullptr;
int draw_map(ScreenFrame& panel) {

    bool regen_mask_flag = false;
//    SDL_Log("Drawing Map ");
    if (!panel.renderer) {
        SDL_Log("Missing Surface!");
        return 1;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return 1;
    }

    // blank the box
    panel.Clear();

    // start with the day map
    SDL_SetRenderTarget(panel.renderer, panel.texture);
    SDL_RenderTexture(panel.renderer, DayMap.texture, NULL, NULL);
//    SDL_Log("Rendered Daymap to texture");

    // init the night map alpha mask
    if (!night_mask || (old_renderer != panel.renderer)) {
        if (night_mask) {
            SDL_DestroySurface(night_mask);
        }
        night_mask = SDL_CreateSurface(panel.dims.w, panel.dims.h, SDL_PIXELFORMAT_RGBA32);
        if (!night_mask) {
            SDL_Log("Failed to create mask surface: %s", SDL_GetError());
            return 1;
        }


        if (map_timer) {
            SDL_RemoveTimer(map_timer);
            map_timer = 0;
        }
        night_mask_args->source = NightMap.surface;
        night_mask_args->dest = night_mask;
        night_mask_args->panel_dims = panel.dims;
        map_timer = SDL_AddTimer(30000, regen_mask, night_mask_args);
        old_renderer = panel.renderer;
//        SDL_Log("Regen NightMask -- bad renderer");
        regen_mask_flag = true;
    }
    if (regen_mask_flag) {
//        SDL_Log("Regen NightMask");
        // calculate the NightMap Alpha mask
         regen_mask (NightMap.surface, night_mask, panel.dims);
    }
    // render the masked NightMap to the panel
//    SDL_Log("render the masked NightMap to the panel");
    SDL_LockMutex(night_mask_mutex);    /// MUTEX LOCK
    SDL_Texture* mask_tex = SDL_CreateTextureFromSurface(panel.renderer, night_mask);
    SDL_UnlockMutex(night_mask_mutex);  /// MUTEX UNLOCK
    if (!mask_tex) {
        SDL_Log("Failed to create mask texture: %s", SDL_GetError());
        return 1;
    }
    //set the blend mode for the alpha overlay of Night Map
    SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
//    SDL_SetRenderTarget(panel.renderer, panel.texture);
    SDL_RenderTexture(panel.renderer, mask_tex, NULL, NULL);
    SDL_DestroyTexture(mask_tex);
    SDL_RenderTexture(panel.renderer, CountriesMap.texture, NULL, NULL);

    // draw equator and tropics
    SDL_SetRenderDrawColor(panel.renderer, 128,128,128,64);
    SDL_RenderLine(panel.renderer, 0,(panel.dims.h/2), panel.dims.w, (panel.dims.h/2));
    SDL_SetRenderDrawColor(panel.renderer, 128,0,0,64);
    int tropic;
    tropic = ((-23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(panel.renderer, 0,tropic, panel.dims.w, tropic);
    tropic = ((23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(panel.renderer, 0,tropic, panel.dims.w, tropic);

//    SDL_Log("draw map pins");
    if (map_pins) {
        struct map_pin* current_pin;
        current_pin=map_pins;
        while (current_pin) {
//            SDL_Log("rendering pin %i, %i, %i, %i for %i", current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a, current_pin->owner);
            render_pin(&panel, current_pin);
            current_pin=current_pin->next;
        }
    }

//    SDL_Log("Drawing Map Complete");
    return 0;
}

