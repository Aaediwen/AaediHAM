#include "../aaediclock.h"
#include "lunar.h"
#include <SDL3_image/SDL_image.h>

struct {
    time_t timestamp 	= 0;
    double fraction 	= 0.0;
    double angle	= 0.0;
    double i		= 0.0;
} moon_illumination;

double moon_phase_angle(const time_t& t) {
    double jd =  (t / 86400) + 2440587.5;
    // Days since known new moon (Jan 6, 2000 18:14 UT)
    double D = jd - 2451550.1;
    double synodic_month = 29.53058867;

    // Phase as fraction of synodic month [0,1)
    double phase = fmod(D, synodic_month) / synodic_month;
    if (phase < 0) phase += 1.0;

    // Convert to phase angle [0, 2π]
    return phase * 2.0 * M_PI;
}

SDL_Surface* gen_moon_phase_mask(SDL_Renderer* renderer, SDL_FRect size) {
    SDL_Surface* result = nullptr;
    if (moon_illumination.timestamp) {
//        moon_illumination.angle=M_PI;
        result = SDL_CreateSurface(size.w, size.h, SDL_PIXELFORMAT_RGBA32);
//        SDL_Log("Illumination Percent\t %3.3f", moon_illumination.fraction *100);
//        SDL_Log("Illumination Angle\t %3.3f", moon_illumination.angle);
//        SDL_Log("Timestamp\t %lld", static_cast<long long>(moon_illumination.timestamp));
        if (result) {
            float x, y;
            float cx=size.w/2.0;
            float cy=size.h/2.0;
            SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
            Uint8* alpha_pixels = (Uint8*)result->pixels;
            const SDL_PixelFormatDetails* dest_details = SDL_GetPixelFormatDetails(result->format);
            const Uint8 dest_bpp = dest_details->bytes_per_pixel;
//            SDL_Log("Moon illumination: %f", moon_illumination.fraction);
            int alpha = 0;
            int red = 0;
            double r = cx;
            for (y = 0 ; y < size.h ; y++) {
                for (x = 0 ; x < size.w ; x++) {
                    float dx = x-cx;
                    float dy = y-cy;
                    double value = sqrt((dx)*(dx) + (dy)*(dy));
                    if (value < r) {
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
                        double chord_half = sqrt((r*r) - (yr*yr));

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
////                        // inside if the lunar disc
                    } else {
                        red = 0;
                        alpha = 255; // outside of the lunar disc
                    }
// old one =======================
/*
            double theta = -1*moon_illumination.angle;
            theta +=45.0;
                    if (value < (r*r)) {
                        // inside the lunar disc
                        double px_pct = value / (r*r);
                        // rotate coords
//                        dx *=-1;
                        double xr =  dx * cos(theta) + dy * sin(theta);
                        double yr =  -dx * sin(theta) + dy * cos(theta);
                        double cut = ((2.0*moon_illumination.fraction)-1.0) * sqrt((r*r) - (yr*yr));
                        if (xr <= cut) {
                            //illuminated
                            alpha = 255;
                        } else {
                            // shadowd
                            alpha = 32;
                            // alpha=96;
                        }


                    } else {
                        alpha=0;
                    }
*/
// end old one
                    int dest_pixel_index =   ( result->w * dest_bpp * y ) + ( dest_bpp * x );
                    Uint32 dst_pixel_val = SDL_MapRGBA(dest_details, NULL, red, 0, 0, (alpha));
                    memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);
                }
             }

        } else {
             debug_log << "MOON: Error Creating Moon Phase Alpha mask: " << SDL_GetError() << "\n";
             SDL_Log ("No MOON MASK!");
        }
    } else {
        SDL_Log ("No lunar timestamp! skipping mask");
    }
    return (result);
}

struct GeoCoord sublunar(const time_t time) {
    struct GeoCoord result;
    // convert to Julian Centuries since J2000 (January 2000)
     // divide Unix Time by seconds per day(86400), and adjust offset to 01-01-1970 (2440587.5)

    double jd =  (static_cast<double>(time) / 86400) + 2440587.5;	// convert to Julian date
     // adjust again to January 2000 and divide by 36525 days/Julian century (MESSUS P 151 24.1) T
     // error of 0.00001 in T == 0.37 days
    double T = (jd - 2451545.0) / 36525.0;	// Time to Julian Centuries
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
    double A1 = 119.75 + (131.849 * T);
    double A2 = 53.09 +  (479264.290 * T);
    double A3 = 313.45 + (481266.484 * T);
    // Meeus P 308 45.6
    double E = 1- (0.002516 * T) - (0.0000074 * T * T);



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
    double eps = (23.0 + (26.0/60.0) + (21.448/3600.0) )
                    - ((46.8150/3600.0) * T)
                    - ((0.00059/3600.0) * T * T)
                    + ((0.001813/3600.0) * T * T * T);
    eps = eps*M_PI/180.0;
    lon = lon*M_PI/180.0;
    lat = lat*M_PI/180.0;

// right ascension / declination

    double x = cos(lon) * cos(lat);
    double y = sin(lon) * cos(lat) * cos(eps) - sin(lat) * sin(eps);
    double z = sin(lon) * cos(lat) * sin(eps) + sin(lat) * cos(eps);

    double RA  = atan2(y, x);       // radians
    double Dec = asin(z);

    // illuminated fraction of the moon
    // Meeus 46.4 p 316
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
    SDL_Log ("Moon arguments:\nL0\t%f",L0 );
    SDL_Log ("D\t%f\tD_rad:\t%f", D, D_rad);
    SDL_Log ("M\t%f\tM_rad:\t%f", M, M_rad);
    SDL_Log ("M0\t%f\tM)_rad:\t%f", M0, M0_rad);
    SDL_Log ("F\t%f\tF_rad:\t%f", F, F_rad);
    SDL_Log ("i\t%f\ti_rad:\t%f", i, i_rad);
    SDL_Log ("illumination: %f", moon_illumination.fraction);
    SDL_Log ("Time: %zu", time);

    //Meeus 46.5 P 316
    double RA_Delta = g_celestials.sun.RA - RA;
    double numerator = cos(g_celestials.sun.Dec) * sin (RA_Delta);
    double denominator = (sin(g_celestials.sun.Dec) * cos(Dec))
                        - cos(g_celestials.sun.Dec) * sin(Dec)
                        * cos(RA_Delta);
    moon_illumination.angle = atan2(numerator, denominator);
    // ------ Need to replace this with something better later ------------
    // for now, let's just clamp it to the horizontal
    // until I can figure out proper math, at least this works
    if (i >0) {
        moon_illumination.angle = 2*M_PI;
    } else {
        moon_illumination.angle = M_PI;
    }
    moon_illumination.i=i;
    //--------------------------------------------------------------------
     // adjust coordinate system from Celestial to geographical relative to Greenwich
//      Meesus P89
        // Greenwich Mean Sidereal Time (deg)
    double d = jd - 2451545.0;
    double GMST = fmod(280.46061837 + 360.98564736629 * d, 360.0);
    if (GMST < 0) GMST += 360.0;

    double lon_sublunar = (GMST*M_PI/180.0) - RA;  // Earth-fixed longitude
    if (lon_sublunar >  M_PI) lon_sublunar -= 2*M_PI;
    if (lon_sublunar < -M_PI) lon_sublunar += 2*M_PI;

    double lat_sublunar = Dec;


    result.latitude = lat_sublunar*180.0/M_PI;
    result.longitude = lon_sublunar*180.0/M_PI;
     g_celestials.moon.timestamp=time;
     g_celestials.moon.Lat = result.latitude;
     g_celestials.moon.Lon = result.longitude;
     g_celestials.moon.RA  = RA;
     g_celestials.moon.Dec = Dec;
    //        L0  D   DR  M   MR  M0 M0R  F   FR  i   iR  %   T   RAd  num dem ang
//    SDL_Log ("CSV, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %zu, %f, %f, %f, %f",
//             L0, D, D_rad, M, M_rad, M0, M0_rad, F, F_rad, i, i_rad,
//             moon_illumination.fraction, time, RA_Delta, numerator, denominator, moon_illumination.angle);
    return (result);
}


SDL_Surface* moon_image = nullptr;
SDL_Texture* moon_texture = nullptr;
time_t moon_surface_age = 0;
void kill_moon_surface () {
        if (moon_image) {
            SDL_Log ("Killing old Moon Image!");
            SDL_DestroySurface(moon_image);
            moon_image = nullptr;
        }
        if (moon_texture) {
            SDL_Log ("Killing old Moon Texture!");
            SDL_DestroyTexture(moon_texture);
            moon_texture = nullptr;
        }
}
void lunar_module(ScreenFrame& panel, time_t timestamp) {
    float unitx=panel.dims.w/20;
    float unity=panel.dims.h/20;
    if (timestamp == 0) {
        timestamp = time(NULL);
        if (timestamp - moon_surface_age > 300) {
            kill_moon_surface();
        }
    } else {
        kill_moon_surface();
    }

    struct GeoCoord sublunar_point = sublunar(timestamp);
    if (!moon_image) {
        SDL_Surface* image_surface = IMG_Load("images/PIA14011.jpg");
//        if ((panel.dims.w < (image_surface->w/2)) && (panel.dims.h < (image_surface->h/2))) {
            moon_image = SDL_CreateSurface((image_surface->w), (image_surface->h), SDL_PIXELFORMAT_RGBA32);
            moon_surface_age=timestamp;
//        } else {
//            moon_image = SDL_CreateSurface((panel.dims.w*2), (panel.dims.h*2), SDL_PIXELFORMAT_RGBA32);
//        }
//          if (moon_image) {
        if (moon_image && image_surface) {
            if (SDL_BlitSurfaceScaled(image_surface, NULL, moon_image, NULL, SDL_SCALEMODE_LINEAR)) {
                SDL_FRect iconsize;
                iconsize.w = moon_image->w;
                iconsize.h = moon_image->h;
                SDL_Surface* moon_mask =  gen_moon_phase_mask(panel.GetRenderer(), iconsize);
                if (moon_mask) {
                    SDL_BlitSurface(moon_mask, nullptr, moon_image, nullptr);
                    SDL_DestroySurface(moon_mask);
                } else {
                    SDL_Log ("We have no MOON MASK in the parent!");
                }
                SDL_SetSurfaceColorKey(moon_image, 1, 0);
                icon_bin.set_dynamic(panel.GetRenderer(), moon_image, map_icons::ICON_MOON);
            }
        } else {
            SDL_Log ("Missing Moon image pr image surface");
            if (image_surface) {
                SDL_Log("Loaded image from BMP");
            }
            if (moon_image) {
                SDL_Log("Loaded moon image");
            }
        }
        if (image_surface) { SDL_DestroySurface(image_surface); }
    }
    if (!moon_texture) {
        moon_texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), moon_image);
    }
    panel.Clear();
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_RenderTexture(panel.GetRenderer(), moon_texture, NULL, NULL);
    char boxtext[64];
    sprintf (boxtext, "Ill: %2.2f\%", (moon_illumination.fraction*100));
    panel.render_text(SDL_FRect{unitx,unity,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
    SDL_Log (boxtext);
    sprintf (boxtext, "Ang: %3.3f\%", (moon_illumination.angle));
    panel.render_text(SDL_FRect{unitx,unity+unity,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
    SDL_Log (boxtext);
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
    panel.render_text(SDL_FRect{unitx*10,unity,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
    sprintf (boxtext, "T: %lld", static_cast<long long>(timestamp));
    panel.render_text(SDL_FRect{unitx,unity*19,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
    struct tm* clocktime = gmtime(&timestamp);
    strftime(boxtext, sizeof(boxtext), "%Y-%m-%d", clocktime);
    panel.render_text(SDL_FRect{unitx*10,unity*19,unitx*8,unity}, Sans, SDL_Color{255,128,128,0}, boxtext);
    SDL_Log (boxtext);
    SDL_Log: Debug:
    struct map_pin moon_pin;
    moon_pin.owner=MOD_LUNAR;
    moon_pin.lat=sublunar_point.latitude;
    moon_pin.lon=sublunar_point.longitude;
    moon_pin.color=SDL_Color{255,255,0,255};
    moon_pin.tooltip[0]=0;
    delete_owner_pins(MOD_LUNAR);
    moon_pin.icon = icon_bin.get_icon(map_icons::ICON_MOON);
    if (!moon_pin.icon) {
        icon_bin.set_dynamic(panel.GetRenderer(), moon_image, map_icons::ICON_MOON);
         moon_pin.icon = icon_bin.get_icon(map_icons::ICON_MOON);
    }
    add_pin(&moon_pin);


    return;
}