#include "../aaediclock.h"
#include "../utils.h"

void load_maps(SDL_Renderer* surface, SDL_FRect size) {
    debug_log << "MAP: Reloading Maps\n";
    DayMap.Reset();
    NightMap.Reset();
    CountriesMap.Reset();
    SDL_Surface* temp_surface = nullptr;
//    DayMap.surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    temp_surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    if (temp_surface) {
        DayMap.surface = SDL_CreateSurface(size.w, size.h, SDL_PIXELFORMAT_RGBA32);
        if (DayMap.surface) {
            if (!SDL_BlitSurfaceScaled(temp_surface, NULL, DayMap.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                debug_log << "MAP: Error scaling Day Map to DayMap surface!\n";
                SDL_DestroySurface(DayMap.surface);
                DayMap.surface = nullptr;
            }
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    }
    if (DayMap.surface) {
        DayMap.texture = SDL_CreateTextureFromSurface(surface,DayMap.surface);
        if (!DayMap.texture) {
            SDL_Log("Unable to load DayMap Texture: %s\n", SDL_GetError());
            debug_log << "Unable to load DayMap Texture: " << SDL_GetError() << "\n";
            exit(1);
        } else {
            int w = DayMap.surface->w;
            int h = DayMap.surface->h;
            int bpp = 4;
            double surf_size_kb = (DayMap.surface->pitch * DayMap.surface->h) / 1024.0;
            double tex_size_kb = (w * h * 4.0) / 1024.0; // assuming RGBA8888
            debug_log << "MAP: Created DayMap texture "
              << w << "x" << h << " "
              << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
              << "=> GPU texture ≈ " << tex_size_kb << " KB "
              << "at " << static_cast<void*>(DayMap.texture) << "\n";
        }
    } else {
        SDL_Log("Unable to load DayMap Surface: %s\n", SDL_GetError());
        debug_log << "Unable to load DayMap Surface: " << SDL_GetError() << "\n";
        exit(1);
    }
//    NightMap.surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    temp_surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    if (temp_surface) {
        NightMap.surface = SDL_CreateSurface(size.w, size.h, SDL_PIXELFORMAT_RGBA32);
        if (NightMap.surface) {
            if (!SDL_BlitSurfaceScaled(temp_surface, NULL, NightMap.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                debug_log << "MAP: Error scaling Night Map to NightMap surface!\n";
                SDL_DestroySurface(NightMap.surface);
                NightMap.surface = nullptr;
            }
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    }
    if (NightMap.surface) {
        NightMap.texture = SDL_CreateTextureFromSurface(surface,NightMap.surface);
        if (!NightMap.texture) {
            SDL_Log("Unable to load NightMap Texture: %s\n", SDL_GetError());
            debug_log << "Unable to load NightMap Texture: " << SDL_GetError() << "\n";
        } else {
            int w = NightMap.surface->w;
            int h = NightMap.surface->h;
            int bpp = 4;
            double surf_size_kb = (NightMap.surface->pitch * NightMap.surface->h) / 1024.0;
            double tex_size_kb = (w * h * 4.0) / 1024.0; // assuming RGBA8888
            debug_log << "MAP: Created NightMap texture "
              << w << "x" << h << " "
              << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
              << "=> GPU texture ≈ " << tex_size_kb << " KB "
              << "at " << static_cast<void*>(NightMap.texture) << "\n";
        }
    } else {
        SDL_Log("Unable to load NightMap Surface: %s\n", SDL_GetError());
        debug_log << "Unable to load NightMap Surface: " << SDL_GetError() << "\n";
        exit(1);
    }

    if ((!DayMap.texture) || (!NightMap.texture)) {
        SDL_Log("Unable to load Maps");
    }

    SDL_SetTextureBlendMode(DayMap.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(NightMap.texture, SDL_BLENDMODE_NONE);
    temp_surface = SDL_LoadBMP("images/outline.bmp");
    if (temp_surface) {
        CountriesMap.surface = SDL_CreateSurface(size.w, size.h, SDL_PIXELFORMAT_RGBA32);
        if (CountriesMap.surface) {
            if (!SDL_BlitSurfaceScaled(temp_surface, NULL, CountriesMap.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                debug_log << "MAP: Error scaling Countries Map to CountriesMap surface!\n";
                SDL_DestroySurface(CountriesMap.surface);
                CountriesMap.surface = nullptr;
            }
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    }
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
                debug_log << "Unable to load Country texture: " << SDL_GetError() << "\n";
                exit(1);
        }  else {
            double surf_size_kb = (CountriesMap.surface->pitch * CountriesMap.surface->h) / 1024.0;
            double tex_size_kb = (CountriesMap.surface->w * CountriesMap.surface->h * 4.0) / 1024.0; // assuming RGBA8888
            debug_log << "MAP: Created CountriesMap texture "
              << x << "x" << y << " "
              << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
              << "=> GPU texture ≈ " << tex_size_kb << " KB "
              << "at " << static_cast<void*>(CountriesMap.texture) << "\n";
            debug_log.flush();
        }
    } else {
        SDL_Log("MAP: Unable to load Country Surface: %s\n", SDL_GetError());
        exit(1);
    }
//    SDL_Log("ALL MAPS LOADED %s\n", SDL_GetError());
    return;

}

void render_pin(ScreenFrame *panel, struct map_pin *current_pin) {

    SDL_Texture* icon_tex = nullptr;
    SDL_FRect target_rect;
    int unit_scale = (panel->dims.w/100);
    if (current_pin->icon) {
        icon_tex = SDL_CreateTexture(panel->GetRenderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, unit_scale*4, unit_scale*4);
        if (!icon_tex) {
            debug_log << "MAP: Failed to create icon texture: " << SDL_GetError() << "\n";
            return ;
        }
        // render the icon
        SDL_SetRenderTarget(panel->GetRenderer(), icon_tex);
        // clear the icon
        SDL_SetRenderDrawColor(panel->GetRenderer(), 0, 0, 0, 0);
        SDL_RenderClear(panel->GetRenderer());
        SDL_RenderTexture(panel->GetRenderer(), current_pin->icon, NULL, NULL);
        target_rect.h=unit_scale*2;
        target_rect.w=unit_scale*2;
    } else {
        icon_tex = SDL_CreateTexture(panel->GetRenderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, unit_scale, unit_scale);
        if (!icon_tex) {
            debug_log << "MAP: Failed to create icon texture: " <<  SDL_GetError() << "\n";
            return ;
        }
    // render the icon
         SDL_SetRenderTarget(panel->GetRenderer(), icon_tex);
         // clear the icon
         SDL_SetRenderDrawColor(panel->GetRenderer(), 0, 0, 0, 0);
         SDL_RenderClear(panel->GetRenderer());

         SDL_FRect pin_rect = {4.0f, 4.0f, 8.0f, 8.0f};
         pin_rect = { unit_scale/5.0f, unit_scale/5.0f, unit_scale*0.6f, unit_scale*0.6f };
         SDL_SetRenderDrawColor(panel->GetRenderer(), 16, 16, 16, 128);
         SDL_RenderFillRect(panel->GetRenderer(), NULL);
         SDL_SetRenderDrawColor(panel->GetRenderer(), current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a);
         SDL_RenderFillRect(panel->GetRenderer(), &pin_rect );
         target_rect.h=unit_scale/2;
         target_rect.w=unit_scale/2;
    }


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
    SDL_SetRenderTarget(panel->GetRenderer(), panel->texture);
    SDL_RenderTexture(panel->GetRenderer(), icon_tex, NULL, &target_rect);
    SDL_SetRenderTarget(panel->GetRenderer(), NULL);
    SDL_DestroyTexture(icon_tex);
    return;
}

void regen_mask (SDL_Surface* source, SDL_Surface* dest, const SDL_FRect& panel_dims) {
    time_t nowtime = time(nullptr);
    struct tm utc;
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Regen Map Mask during resize event!");
        return;
    }
//    tm utcroot; // check these on Linux
#ifdef _WIN32
    gmtime_s(&utc, &nowtime);
#else
    gmtime_r(&nowtime, &utc);
#endif
    SDL_Rect panel_cords, source_cords;
    double softness = 10.0;
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc.tm_yday+1)) ));
    debug_log <<"MAP: Regen Terminator Alpha Mask — source: "
            << (void*)source << ", dest: " << (void*)dest
            <<", dims: " << panel_dims.w << "x" << panel_dims.h << "\n";

    SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK

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

    SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);  /// MUTEX UNLOCK
    return;
}

Uint32 SDLCALL regen_mask (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    if (timerID) {
        struct regen_mask_args* args = (struct regen_mask_args*)userdata;
//        SDL_Log("Timer Callback — source: %p, dest: %p, dims: %.1fx%.1f",
//            (void*)args->source, (void*)args->dest,
//            args->panel_dims.w, args->panel_dims.h);
        regen_mask (args->source, args->dest, args->panel_dims);
        return (30000);
    } else {
        return 0;
    }
}

SDL_Surface* night_mask = nullptr;
SDL_Renderer* old_renderer = nullptr;
int draw_map(ScreenFrame& panel) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("MAP DRaw during resize event!");
        return (0);
    }
    bool regen_mask_flag = false;
//    SDL_Log("Drawing Map ");
    if (!panel.GetRenderer()) {
        debug_log << "MAP: Missing Surface!\n";
        return 1;
    }
    if (!panel.texture) {
        debug_log << "MAP: Missing PANEL!\n";
        return 1;
    }

    // blank the box
    panel.Clear();
    if ((!NightMap.surface) || (!DayMap.surface) || (!CountriesMap.surface)) {
        debug_log << "Missing Map Textures!\n";
        exit(1);
    }
    // start with the day map
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_RenderTexture(panel.GetRenderer(), DayMap.texture, NULL, NULL);

    // init the night map alpha mask
    if (!night_mask ||
        (old_renderer != panel.GetRenderer()) ||
        night_mask->w != floor(panel.dims.w) ||
        night_mask->h != floor(panel.dims.h)) {
        if (night_mask) {
            debug_log << "MAP: New Night Mask -- Night Mask Dims: "<<night_mask->w<<"x"<<night_mask->h<<"\tPanel: "<<panel.dims.w<<"x"<<panel.dims.h<< "\n";
            SDL_DestroySurface(night_mask);
        }
        night_mask = SDL_CreateSurface(panel.dims.w, panel.dims.h, SDL_PIXELFORMAT_RGBA32);
        if (!night_mask) {
            debug_log << "MAP: Failed to create mask surface: " << SDL_GetError() << "\n";
            return 1;
        }

        if ((NightMap.dims.w < DayMap.dims.w) || (NightMap.dims.h < DayMap.dims.h)) {
            SDL_Log("Map renderer reloading maps");
            debug_log << "MAP: Map renderer reloading maps\n";
           load_maps(panel.GetRenderer(), panel.dims);
        }
        if (map_timer) {
            SDL_RemoveTimer(map_timer);
            map_timer = 0;
        }
        night_mask_args->source = NightMap.surface;
        night_mask_args->dest = night_mask;
        night_mask_args->panel_dims = panel.dims;
        map_timer = SDL_AddTimer(30, regen_mask, night_mask_args);
        debug_log << "MAP: Regen NightMask -- bad renderer\t Old: "<< old_renderer << "\tNew: "<< panel.GetRenderer() <<"\n";
        old_renderer = panel.GetRenderer();

        regen_mask_flag = true;
    }
    if (regen_mask_flag) {
        debug_log << "MAP: Regen NightMask\n";
        // calculate the NightMap Alpha mask
         regen_mask (NightMap.surface, night_mask, panel.dims);
    }
    // render the masked NightMap to the panel
//    SDL_Log("render the masked NightMap to the panel");
    SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
    SDL_Texture* mask_tex = SDL_CreateTextureFromSurface(panel.GetRenderer(), night_mask);
    SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);  /// MUTEX UNLOCK
    if (!mask_tex) {
        debug_log << "Failed to create mask texture: " << SDL_GetError() << "\n";
        return 1;
    }
    //set the blend mode for the alpha overlay of Night Map
    SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
//    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_RenderTexture(panel.GetRenderer(), mask_tex, NULL, NULL);
    SDL_DestroyTexture(mask_tex);
    SDL_RenderTexture(panel.GetRenderer(), CountriesMap.texture, NULL, NULL);

    // draw equator and tropics
    SDL_SetRenderDrawColor(panel.GetRenderer(), 128,128,128,64);
    SDL_RenderLine(panel.GetRenderer(), 0,(panel.dims.h/2), panel.dims.w, (panel.dims.h/2));
    SDL_SetRenderDrawColor(panel.GetRenderer(), 128,0,0,64);
    int tropic;
    tropic = ((-23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(panel.GetRenderer(), 0,tropic, panel.dims.w, tropic);
    tropic = ((23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(panel.GetRenderer(), 0,tropic, panel.dims.w, tropic);

    debug_log << "MAP: draw map pins\n";
    if (map_pins) {
        struct map_pin* current_pin;
        current_pin=map_pins;
        while (current_pin) {
            render_pin(&panel, current_pin);
            current_pin=current_pin->next;
        }
    }

    overlays.reset_index();
    ScreenFrame* overlay = overlays.next_overlay();
    while (overlay) {
         SDL_SetTextureBlendMode(overlay->texture, SDL_BLENDMODE_BLEND);
         SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
         SDL_RenderTexture(panel.GetRenderer(), overlay->texture, NULL, NULL);
         overlay = overlays.next_overlay();
    }

//    SDL_Log("Drawing Map Complete");
    return 0;
}

