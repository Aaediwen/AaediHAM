#include "../aaediclock.h"
#include "lunar.h"
#include <SDL3_image/SDL_image.h>

struct {
    time_t timestamp 	= 0;
    double fraction 	= 0.0;
    double angle	= 0.0;
    double i		= 0.0;
} moon_illumination;

namespace LunarConstants {
    constexpr double JD_UNIX_EPOCH 		= 2440587.5; 	// offset to 01-01-1970 (2440587.5)
    constexpr double SECONDS_PER_DAY 		= 86400.0;   	// seconds per calendar day
    constexpr double DAYS_PER_JULIAN_CENTURY	= 36525.0;	// days per Julian Century
    constexpr double SYNODIC_MONTH		= 29.53058867;  // synodic month
    constexpr double J2000			= 2451545.0;	// Julian Date of J2000.0, which is January 1, 2000 at 12:00 TT
    constexpr double NEW_MOON			= 2451550.1;	// Julian Date of known new moon (Jan 6, 2000 18:14 UT)
    constexpr double OBLIQUITY_J2000		= 23.4392911;	// (23.0 + (26.0/60.0) + (21.448/3600.0) )  23°26'21.448"
}
// may need to restore this function later to take a crack at non-hard coded phase angle
/*double moon_phase_angle(const time_t& t) {
    double jd =  (t / LunarConstants::SECONDS_PER_DAY) + LunarConstants::JD_UNIX_EPOCH;
    // Days since known new moon (Jan 6, 2000 18:14 UT)
    double D = jd - LunarConstants::NEW_MOON;

    // Phase as fraction of synodic month [0,1)
    double phase = fmod(D,  LunarConstants::SYNODIC_MONTH) / LunarConstants::SYNODIC_MONTH;
    if (phase < 0) phase += 1.0;

    // Convert to phase angle [0, 2π]
    return phase * 2.0 * M_PI;
}
*/

SDL_Surface* gen_moon_phase_mask(SDL_FRect size) {

/*
    function to generate the lunar phase mask bitmap
    according to the data in the moon_illumination global
    and SDL_FRect size
*/
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Gen Moon Phase Mask during resize event!");
        return (nullptr);
    }
    if (size.w < 1.0 || size.h < 1.0) {
        debug_log << "LUNAR: Invalid MASK DIMS\n";
        return (nullptr);
    }
    SDL_Surface* result = nullptr;
    if (moon_illumination.timestamp) {
        // moon_illumination global is valid
        result = SDL_CreateSurface(static_cast<int>(size.w), static_cast<int>(size.h), SDL_PIXELFORMAT_RGBA32);
        debug_log << "LUNAR: Illumination Percent\t  " << moon_illumination.fraction *100 << "\n";
        debug_log << "LUNAR: Illumination Angle\t  " << moon_illumination.angle << "\n";
        debug_log << "LUNAR: Timestamp\t  " << moon_illumination.timestamp << "\n";
        if (result) {
            // we were able to create the target surface
            // now to render the mask to it

            const double cx=size.w/2.0f;
            const double cy=size.h/2.0f;
            const double r = cx;
            const SDL_PixelFormatDetails* dest_details = SDL_GetPixelFormatDetails(result->format);
            const Uint8 dest_bpp = dest_details->bytes_per_pixel;
            Uint8* alpha_pixels = (Uint8*)result->pixels;
            SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);

            float x, y;
            Uint8 alpha = 0;
            Uint8 red = 0;
            // calculate the mask pixel by pixel
            for (y = 0 ; y < size.h ; y++) {
                for (x = 0 ; x < size.w ; x++) {
                    double dx = x-cx;
                    double dy = y-cy;
                    double value = sqrt((dx)*(dx) + (dy)*(dy));
                    if (value < r) {
                        // inside if the lunar disc
                        // rotate point by moon illumination angle
                        double xr = (dx * cos(moon_illumination.angle)) - (dy * sin(moon_illumination.angle));
                        double yr = (dx * sin(moon_illumination.angle)) + (dy * cos(moon_illumination.angle));
                        /*
////                        at each Y coordinate of the image,
////                        we look at the rotated chord of the moon disc as a 1D object.
////                        then we shade it based on what percent of the line is represented
////                        by our current XR position as a linear scale.
////
////                        Since we are only looking at the chord of the circle defined by YR,
////                        the percentage shifts automagically according to our Y position
////                        and generates the appropriate crecent or gibbous shape.
////
////                        cut here, means the last X coordinate on the chord
////                        to the limit of the surface X resolution that represents
////                        moon_illumination.fraction of the chord.  We set our alpha
////                        mask according to are we greater or less than this point
////                        accross the rotation surface of the moon for each pixel of the image
                        */
                        // attempting to trap negative sqrt == NaN possability
                        double arg = (r*r) - (yr * yr);
                        if ( arg <= 0.0 ) arg = 0.0;
                        double chord_half = sqrt(arg);

                        double cut = ((2.0*moon_illumination.fraction)-1.0) * chord_half;
////                        if ((moon_illumination.fraction < 0.75) && (moon_illumination.fraction > 0.25)) {
////                        if (((int)floor(x) % 10)==0) {
////                            SDL_Log("Cut at XR=CX\t %3.5f\t YR\t %3.5f\t Chord Half: %3.5f", cut, yr, chord_half);
////                            SDL_Log("DX, DY\t %5.5f, %5.5f\t XR, YR\t %5.5f, %5.5f\t R: %5.5f", dx, dy, xr, yr, r);
////                        }
////                        }
                        if (xr <= cut) {
////                            //illuminated
////                            SDL_Log("LIT PIXEL!");
                            alpha = 0;
                        } else {
////                            // shadowd
////                            //alpha = 32;
////                            SDL_Log("Shaded Pixel!");
                            alpha=196;
                        }
////
////
                    } else {
                        red = 0;
                        alpha = 255; // outside of the lunar disc
                    }
                    // copy the pixel value into place
                    int dest_pixel_index =   static_cast<int>(( result->w * dest_bpp * y ) + ( dest_bpp * x ));
                    Uint32 dst_pixel_val = SDL_MapRGBA(dest_details, NULL, red, 0, 0, (alpha));
                    memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);
                }
             }
             debug_log << "LUNAR: Done Creating Moon Phase Alpha mask \n";
        } else {
             debug_log << "LUNAR: Error Creating Moon Phase Alpha mask: " << SDL_GetError() << "\n";
             SDL_Log ("No MOON MASK!");
             return (nullptr);
        }
    } else {
        // moon_illumination global is invalid
        SDL_Log ("No lunar timestamp! skipping mask");
    }
    return (result);
}

struct GeoCoord sublunar(const time_t time) {
    // function to calculate the sublunar position given a unix time
    struct GeoCoord result;

    //================================================================================
    // Calculate Lunar Arguments
    //================================================================================
    // convert to Julian Centuries since J2000 (January 2000)
     // divide Unix Time by seconds per day(86400), and adjust offset to 01-01-1970 (2440587.5l)

    double jd =  (static_cast<double>(time) / LunarConstants::SECONDS_PER_DAY) + LunarConstants::JD_UNIX_EPOCH;	// convert to Julian date
     // adjust again to January 2000 and divide by 36525 days/Julian century (MESSUS P 151 24.1) T
     // error of 0.00001 in T == 0.37 days
    double T = (jd - LunarConstants::J2000) / LunarConstants::DAYS_PER_JULIAN_CENTURY;	// Time to Julian Centuries
    // L' D M M' F
    // moon mean longitude (Meeus P 308 45.1)
    double L0 = fmod(218.3164591 + (481267.88134236 * T)
                     - (0.0013268 * T * T) + ( (T * T * T)/538841 )
                     - ((T * T * T * T)/65194000),360);
    // mean elongation of the moon (Meeus P 308 45.2)
    double D = fmod(297.8502042 + ( 445267.1115168 * T )
                   - (0.00163 * T * T) + ((T*T*T)/545868) - ((T*T*T*T)/113065000),360);
    // suns mean anomaly (Meeus P 308 45.3)
    double M = fmod(357.5291092 + (35999.0502909 * T)
                  - (0.0001536 * T * T) + (( T * T * T ) / 24490000),360);
    // moon's mean anomaly (Meeus P 308 45.4)
    double M0 = fmod(134.9634114 + (477198.8676313 *T)
                + (0.0089970 * T * T) + ((T*T*T)/69699)
                - (T*T*T*T)/14712000 , 360);
    // moon's argument of latitude (mean distance of the moon from its ascending node)
    // Meeus P308 45.5
    double F = fmod(93.2720993 + (483202.0175273 * T)
                   - (0.0034029 * T * T) - ((T*T*T)/3526000) + ((T*T*T*T)/863310000),360);
    // convert arguments deg to rad
    double M_rad = M*M_PI/180.0;
    double F_rad = F*M_PI/180.0;
    double D_rad = D*M_PI/180.0;
    double M0_rad = M0*M_PI/180.0;
//    double A1 = 119.75 + (131.849 * T);
//    double A2 = 53.09 +  (479264.290 * T);
//    double A3 = 313.45 + (481266.484 * T);
    // Meeus P 308 45.6
//    double E = 1- (0.002516 * T) - (0.0000074 * T * T);


//================================================================================
// MEEUS Correction Tables
//================================================================================

//==============================================================================
// Need to figure out how to re-generate something like the Meeus table for copyright reasons
//==============================================================================
/*
45A	Sum 1 // first few entries from Meeus table 45.A  P309
				0.000,001 deg * 1,000,000
0	0	1	0	6.288774
2	0	-1	0	1.274027
2	0	0	0	0.658314
0	0	2	0	0.213618
0	1	0	0	-0.815116
...

45A  Sum(1)
6.288774  * sin(	D(0)	+	M(0)	+ M'(1)		+ F(0)) +
1.274027  * sin(	D(2)	+	M(0)	+ M'(-1)	+ F(0)) +
0.658314  * sin(	D(2)	+	M(0)	+ M'(0)		+ F(0)) +
0.213618  * sin(	D(0)	+	M(0)	+ M'(2)		+ F(0)) +
-0.815116 * sin(	D(0)	+	M(1)	+ M'(0)		+ F(0)) + ...
// formula described Meeus pp 308 and 312
    double lon = L0
    + 6.289 * sin(M)          // main term // Main perturbation due to the Moon’s own anomaly — largest correction.
    - 1.274 * sin(2*D - M)	// Evection (Moon-Sun gravitational interaction).
    + 0.658 * sin(2*D)		// Variation due to the Moon’s elongation from Sun.
    - 0.214 * sin(2*M)		// Smaller periodic term from Moon’s anomaly.
    - 0.110 * sin(D);		//Additional small term from Moon-Sun geometry.
*/

    double lon = L0
    + 6.289 * sin(M_rad)
    - 1.274 * sin(2*D_rad - M_rad)
    + 0.658 * sin(2*D_rad)
    - 0.214 * sin(2*M_rad)
    - 0.110 * sin(D_rad);
/*
45B Sum b	// first few entries from Meeus table 45B P311
                                0.000,001 deg * 1,000,000
0	0	0	1	5.128122
0	0	1	1	0.280602
0	0	1	-1	0.277693
2	0	0	-1	0.173237
2	0	-1	-1	0.055413
...

45B sum(b)
5.128122 * sin(	D(0)	+	M(0)	+	M'(0)	+ F(1)) +
0.280602 * sin(	D(0)	+	M(0)	+	M'(1)	+ F(1)) +
0.277693 * sin(	D(0)	+	M(0)	+	M'(1)	+ F(-1)) +
0.173237 * sin(	D(2)	+	M(0)	+	M'(0)	+ F(-1)) +
0.055413 * sin(	D(2)	+	M(0)	+	M'(-1)	+ F(-1)) + ...
// formula described Meeus pp 308 and 312
    double lat = 5.128 * sin(F)
    + 0.280 * sin(M + F)
    + 0.277 * sin(M - F)
    + 0.173 * sin(2*D - F);

*/
    double lat = 5.128 * sin(F_rad)
    + 0.280 * sin(M_rad + F_rad)
    + 0.277 * sin(M_rad - F_rad)
    + 0.173 * sin(2*D_rad - F_rad);


/*
     // obliquity of the eleptic per MEESUS 21.2
     // deg + min/60 + sec/3600
     double obliquity = (23.0 + (26.0/60.0) + (21.448/3600.0) )
                    - ((46.8150/3600.0) * T)
                    - ((0.00059/3600.0) * T * T)
                    + ((0.001813/3600.0) * T * T * T);
     double obliquity_rad = obliquity * M_PI / 180.0;

*/
    double eps = LunarConstants::OBLIQUITY_J2000
                    - ((46.8150/3600.0) * T)
                    - ((0.00059/3600.0) * T * T)
                    + ((0.001813/3600.0) * T * T * T);
    eps = eps*M_PI/180.0;
    lon = lon*M_PI/180.0;
    lat = lat*M_PI/180.0;

//================================================================================
// right ascension / declination
//================================================================================
    double x = cos(lon) * cos(lat);
    double y = sin(lon) * cos(lat) * cos(eps) - sin(lat) * sin(eps);
    double z = sin(lon) * cos(lat) * sin(eps) + sin(lat) * cos(eps);

    double RA_rad  = atan2(y, x);       // radians
    double Dec_rad = asin(z);

//================================================================================
// illuminated fraction of the moon
// Meeus 46.4 p 316
//================================================================================

    double i = 180 - D - (6.289 * sin(M0_rad))
                       + (2.100 * sin(M_rad))
                       - (1.274 * sin(2*D_rad - M0_rad))
                       - (0.658 * sin(2*D_rad))
                       - (0.214 * sin(2*M0_rad))
                       - (0.110 * sin(D_rad));
    double i_rad = i*M_PI/180.0;

    // Meeus 46.1 p 315
    moon_illumination.timestamp=time;
    moon_illumination.fraction = (1.0 + cos(i_rad))/2.0;
    debug_log << "LUNAR: Moon arguments : \nL0\t" << L0 << "\n";
    debug_log << "LUNAR: D\t " << D << "\tD_rad:\t " << D_rad << "\n";
    debug_log << "LUNAR: M\t" << M << "\tM_rad:\t" << M_rad << "\n";
    debug_log << "LUNAR: M0\t" << M0 << "\tM0_rad:\t" << M0_rad << "\n";
    debug_log << "LUNAR: F\t" << F << "\tF_rad:\t" << F_rad << "\n";
    debug_log << "LUNAR: i\t" << i << "\ti_rad:\t" << i_rad << "\n";
    debug_log << "LUNAR: illumination: "<< moon_illumination.fraction << "\n";
    debug_log << "LUNAR: Time: "<< time;

    //Meeus 46.5 P 316
    double RA_Delta = g_celestials.sun.RA - RA_rad;
    double numerator = cos(g_celestials.sun.Dec) * sin (RA_Delta);
    double denominator = (sin(g_celestials.sun.Dec) * cos(Dec_rad))
                        - cos(g_celestials.sun.Dec) * sin(Dec_rad)
                        * cos(RA_Delta);
    moon_illumination.angle = atan2(numerator, denominator);
    // ------ Need to replace this with something better later ------------
    // for now, let's just clamp it to the horizontal
    // until I can figure out proper math, at least this works
    if (i >0) {
        moon_illumination.angle = M_PI;
    } else {
        moon_illumination.angle = 2*M_PI;
    }
    moon_illumination.i=i;
    //--------------------------------------------------------------------
    // adjust coordinate system from Celestial to geographical relative to Greenwich
    //      Meesus P89
    // Greenwich Mean Sidereal Time (deg)
    double d = jd - LunarConstants::J2000;
    double GMST = fmod(280.46061837 + 360.98564736629 * d, 360.0);
    if (GMST < 0) GMST += 360.0;

    double lon_sublunar = (GMST*M_PI/180.0) - RA_rad;  // Earth-fixed longitude
    if (lon_sublunar >  M_PI) lon_sublunar -= 2*M_PI;
    if (lon_sublunar < -M_PI) lon_sublunar += 2*M_PI;

    double lat_sublunar = Dec_rad;

//================================================================================
// populate results
//================================================================================

    // convert result to degrees
    result.latitude = lat_sublunar*180.0/M_PI;
    result.longitude = lon_sublunar*180.0/M_PI;
    result.longitude *=-1;
     g_celestials.moon.timestamp=time;
     g_celestials.moon.Lat = result.latitude;
     g_celestials.moon.Lon = result.longitude;
     g_celestials.moon.RA  = RA_rad;
     g_celestials.moon.Dec = Dec_rad;
    //        L0  D   DR  M   MR  M0 M0R  F   FR  i   iR  %   T   RAd  num dem ang
//    SDL_Log ("CSV, %f, %f, %f, %f, %f, %f, %f, %zu",
//             g_celestials.moon.Lat, g_celestials.moon.Lon, g_celestials.moon.RA, g_celestials.moon.Dec, d, jd, GMST, time);
//    SDL_Log ("CSV, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %zu, %f, %f, %f, %f",
//             L0, D, D_rad, M, M_rad, M0, M0_rad, F, F_rad, i, i_rad,
//             moon_illumination.fraction, time, RA_Delta, numerator, denominator, moon_illumination.angle);
    return (result);
}

bool regen_moon_texture = false;
//bool regen_moon_image = true;
SDL_Surface* moon_image = nullptr;
SDL_Texture* moon_texture = nullptr;
time_t moon_surface_age = 0;
SDL_Renderer* old_moon_renderer = nullptr;
SDL_Mutex* moon_mutex = 0;
SDL_TimerID moon_timer = 0;
int moon_max_dims = 0;
int SDLCALL regen_lunar_surface(void* data) {
    (void)data;
//================================================================================
// background routine to reload the lunar images and regen the mask
//================================================================================
   // reload moon image if needed
//    if (regen_moon_image) {
        //================================================================================
        // load the base moon image from disk
        //================================================================================
        float x, y;
        SDL_Surface* image_load = IMG_Load("images/PIA14011.jpg");
        SDL_Surface* image_surface = nullptr;
        SDL_LockMutex(moon_mutex);
        if (image_load) {
            if (moon_max_dims > image_load->w) {
               moon_max_dims = image_load->w;
            }
            if (moon_max_dims <=100) {
               moon_max_dims = image_load->w;
            }
            image_surface = SDL_ScaleSurface(image_load, moon_max_dims, moon_max_dims, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(image_load);
        }

        static int bpp = 4;
        double surf_size_kb;
        double tex_size_kb;
        if (image_surface) { // able to load the image from disk
            // log the success
            x = static_cast<float>(image_surface->w);
            y = static_cast<float>(image_surface->h);
            surf_size_kb = (image_surface->pitch * image_surface->h) / 1024.0;
            tex_size_kb = (x * y * 4.0) / 1024.0; // assuming RGBA8888
            debug_log << "LUNAR: Loaded Moon surface "
                << x << "x" << y << " "
                << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
                << "=> GPU texture ≈ " << tex_size_kb << " KB "
                << "at " << static_cast<void*>(image_surface) << "\n";
            //================================================================================
            // re-create the cached panel moon CPU side SDL_Surface
            //================================================================================
            if (moon_image) {
                SDL_DestroySurface(moon_image);
                moon_image = 0;
            }
            moon_image = SDL_CreateSurface((image_surface->w), (image_surface->h), SDL_PIXELFORMAT_RGBA32);
            if (moon_image) { // able to re-create the moon SDL_Surface
                // log the success
                SDL_ClearSurface(moon_image, 0, 0, 0, 0);
                moon_surface_age = time(NULL);
                x = static_cast<float>(moon_image->w);
                y = static_cast<float>(moon_image->h);
                surf_size_kb = (moon_image->pitch * moon_image->h) / 1024.0;
                tex_size_kb = (x * y * 4.0) / 1024.0; // assuming RGBA8888
                debug_log << "LUNAR: created moon_image surface "
                    << x << "x" << y << " "
                    << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
                    << "=> GPU texture ≈ " << tex_size_kb << " KB "
                    << "at " << static_cast<void*>(image_surface) << "\n";
            }
        }
        //================================================================================
        // We have here been able to load the image from disk
        // and create the new CPU side SDL_Surface to render to
        // now we actually generate the image
        //================================================================================
        if (moon_image && image_surface) {
            // copy the loaded image to our output SDL_Surface
            if (SDL_BlitSurfaceScaled(image_surface, NULL, moon_image, NULL, SDL_SCALEMODE_LINEAR)) {
                // if successful, generate the phase mask
                SDL_FRect iconsize;
                iconsize.w = static_cast<float>(moon_image->w);
                iconsize.h = static_cast<float>(moon_image->h);
                debug_log << "LUNAR: Generating moon mask ... ";
                SDL_Surface* moon_mask =  gen_moon_phase_mask(iconsize);
                if (moon_mask) {
                    // able to create the phase mask, blit it onto our surface
                    debug_log << "Success\n";
                    SDL_BlitSurface(moon_mask, nullptr, moon_image, nullptr);
                    SDL_DestroySurface(moon_mask);
                } else {
                    // missing phase, no phase mask to show
                    debug_log << "LUNAR: We have no MOON MASK in the parent!\n";
                    SDL_Log ("We have no MOON MASK in the parent!");
                }
                // set transparancy
                SDL_SetSurfaceColorKey(moon_image, 1, 0);
                regen_moon_texture = true;
//                regen_moon_image = false;
            }

            if (!moon_texture) {
                regen_moon_texture = true;
            }
        } else {
            //================================================================================
            // Something went wrong loading the image from disk or creating the new SDL_Surface
            //================================================================================
            debug_log << "LUNAR: Missing Moon image or image surface\n";
            if (image_surface) {
                debug_log << "LUNAR: Loaded image from BMP\n";
            }
            if (moon_image) {
                debug_log << "LUNAR: Loaded Moon Image\n";
                SDL_DestroySurface(moon_image);
                moon_image = nullptr;
            }
        }
        // cleanup and exit
        SDL_UnlockMutex(moon_mutex);
        if (image_surface) {
            debug_log << "LUNAR: Cleaning up image_surface\n";
            SDL_DestroySurface(image_surface);
        }
//    } // end reload
    return 0;
}

Uint32 SDLCALL regen_lunar_surface (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(regen_lunar_surface, "Lunar Regen", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "LUNAR: Failed to Create Lunar Regen Thread\n";
          }
          return (30000);
     } else {
          return 0;
     }
}


void lunar_module(ScreenFrame& panel, time_t timestamp) {
    // init
    if (!moon_mutex) {
        moon_mutex= SDL_CreateMutex();
    }
    const Uint64 StartTicks = SDL_GetTicks();
    debug_log << "LUNAR: In Lunar Module\n";
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    } else {
        SDL_Log("Lunar Call during resize event!");
        return ;
    }

    if (!panel.GetRenderer()) {
        debug_log << "LUNAR: Missing Renderer!\n";
        return ;
    }
    if (!moon_timer) {
        int max_w = static_cast<int>(panel.dims.w);
        int max_h = static_cast<int>(panel.dims.h);
        SDL_GetCurrentRenderOutputSize(panel.GetRenderer(), &max_w, &max_h);
        moon_max_dims = max_h;
        if (max_w > max_h) {
            moon_max_dims = max_h;
        }
        moon_max_dims /=4;
        moon_timer = SDL_AddTimer(10, regen_lunar_surface, NULL);
    }
    if (!panel.texture) {
        debug_log << "LUNAR: Missing PANEL!\n";
        return ;
    }
    if (clock_mouse_event.mod_owner == MOD_LUNAR) {
        SDL_Log ("Click event in Lunar module at %f, %f", clock_mouse_event.mod_cords.x, clock_mouse_event.mod_cords.y);
        clock_mouse_event.mod_owner = MOD_NULL;
    }
//    debug_log << "LUNAR: Getting panel units -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
//    debug_log << "LUNAR: getting panel units\n";
    debug_log.flush();
    debug_log << "LUNAR: panel_dims "<< panel.dims.w << " " << panel.dims.h<< "\n";
    debug_log.flush();
    float unitx=panel.dims.w/20;
    float unity=panel.dims.h/20;
    debug_log << "LUNAR: got units\n";
    debug_log.flush();
    if (timestamp == 0) {
        timestamp = time(NULL);
    }
    // get the sublunar point
//    debug_log << "LUNAR: Getting Sublunar point -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
//    debug_log << "LUNAR: Getting Sublunar point\n";
    debug_log.flush();
    struct GeoCoord sublunar_point = sublunar(timestamp);
    SDL_LockMutex(moon_mutex);

    debug_log << "LUNAR: locked moon mutex in parent -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    if ((SDL_GetTicks() - StartTicks) > 200) {
       moon_max_dims -= 10;
       if (moon_max_dims < 100) {
           moon_max_dims = 100;
       }
    }
//    debug_log << "LUNAR: locked moon mutex in parent\n";
    debug_log.flush();
    if (moon_image) {
        if (regen_moon_texture) {
            if (moon_texture) {
                SDL_DestroyTexture(moon_texture);
                moon_texture = 0;
            }
//            icon_bin.set_dynamic(panel.GetRenderer(), moon_image, map_icons::ICON_MOON);
            moon_texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), moon_image);
            regen_moon_texture = false;
        }
    } else {
        if (moon_timer) {
            SDL_RemoveTimer(moon_timer);
        }
        moon_timer = SDL_AddTimer(10, regen_lunar_surface, NULL);
    }
    panel.Clear();
//    debug_log << "LUNAR: cleared panel -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
//    debug_log << "LUNAR: cleared panel\n";
    debug_log.flush();
    SDL_Color lunar_text_color = SDL_Color{ 255,200,200,0 };
    SDL_Color lunar_shadow_color = SDL_Color{ 32,32,32,0 };
    float offsetx=unitx/10;
    float offsety=unity/10;
//    debug_log << "LUNAR: apply the moon image to the panel -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    if (moon_texture) {
        // apply the moon image to the panel
        SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
        SDL_RenderTexture(panel.GetRenderer(), moon_texture, NULL, NULL);
    } else {
        panel.render_text(SDL_FRect{unitx,unity*3,unitx*15,unity*2}, Sans, lunar_text_color, "MISSING MOON TEXTURE");
    }
//    debug_log << "LUNAR: apply text overlays -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    // apply text overlays
    char boxtext[64];
    sprintf (boxtext, "Ill: %2.2f", (moon_illumination.fraction*100));

    panel.render_text(SDL_FRect{unitx+offsetx,unity+offsety,unitx*8,unity}, Sans, lunar_shadow_color, boxtext);
    panel.render_text(SDL_FRect{unitx,unity,unitx*8,unity}, Sans, lunar_text_color, boxtext);
    debug_log << "LUNAR: " << boxtext << "\n";
//    sprintf (boxtext, "Ang: %3.3f\%", (moon_illumination.angle));
//    panel.render_text(SDL_FRect{unitx,unity+unity,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
//    SDL_Log (boxtext);
    if (moon_illumination.fraction > 0.98) {
        sprintf (boxtext, "FULL");
    } else if (moon_illumination.fraction < 0.01) {
        sprintf (boxtext, "NEW");
    } else if ( moon_illumination.fraction > 0.48 && moon_illumination.fraction < 0.52) {
        sprintf (boxtext, "HALF");
    } else if (moon_illumination.fraction < 0.5) {
        if (moon_illumination.i > 0) {
            sprintf (boxtext, "WAXING CRESCENT");
        } else {
            sprintf (boxtext, "WANING CRESCENT");
        }
    } else if (moon_illumination.fraction > 0.5) {
        if (moon_illumination.i > 0) {
            sprintf (boxtext, "WAXING GIBBOUS");
        } else {
            sprintf (boxtext, "WANING GIBBOUS");
        }
    }
    panel.render_text(SDL_FRect{(unitx*10)+offsetx,unity+offsety,unitx*8,unity}, Sans, lunar_shadow_color, boxtext);
    panel.render_text(SDL_FRect{unitx*10,unity,unitx*8,unity}, Sans, lunar_text_color, boxtext);
//    sprintf (boxtext, "T: %lld", static_cast<long long>(timestamp));
//    panel.render_text(SDL_FRect{unitx,unity*19,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
    struct tm* clocktime = gmtime(&timestamp);
    strftime(boxtext, sizeof(boxtext), "%Y-%m-%d", clocktime);
    panel.render_text(SDL_FRect{(unitx*10)+offsetx,(unity*19)+offsety,unitx*8,unity}, Sans, lunar_shadow_color, boxtext);
    panel.render_text(SDL_FRect{unitx*10,unity*19,unitx*8,unity}, Sans, lunar_text_color, boxtext);
//    SDL_Log (boxtext);
//    debug_log << "LUNAR: submit map pin -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    // submit the map pin
    struct map_pin moon_pin;
    moon_pin.owner=MOD_LUNAR;
    sprintf(moon_pin.label, "SUB LUNAR POINT");
    moon_pin.lat=sublunar_point.latitude;
    moon_pin.lon=sublunar_point.longitude;
    moon_pin.color=lunar_text_color;
    moon_pin.tooltip[0]=0;
    delete_owner_pins(MOD_LUNAR);
    moon_pin.icon = 0;
//    moon_pin.icon = icon_bin.get_icon(map_icons::ICON_MOON);
    if (!moon_pin.icon && moon_image) {
//        icon_bin.set_dynamic(panel.GetRenderer(), moon_image, map_icons::ICON_MOON);
//         moon_pin.icon = icon_bin.get_icon(map_icons::ICON_MOON);
        moon_pin.icon = 0;
    }
    SDL_UnlockMutex(moon_mutex);
    add_pin(&moon_pin);
    debug_log << "LUNAR: Done with Lunar Module -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
//    debug_log << "LUNAR: Done with Lunar Module\n";
    debug_log.flush();
    return;
}