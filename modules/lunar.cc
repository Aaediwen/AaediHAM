#include "lunar.h"
#include <SDL3_image/SDL_image.h>

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
    double D = fmod(297.8502042 + ( 481267.88134236 * T )
                   - (0.00163 * T * T) + ((T*T*T)/545868) - ((T*T*T*T)/113065000),360);
    // suns mean anomaly (Meeus P 308 45.3)
    double M = fmod(357.5291092 + (35999.0502909 * T)
                  - (0.0001536 * T * T) + (( T * T * T ) / 24490000),360);
    // moon'd mean anomaly (Meeus P 308 45.4)
    double M0 = fmod(134.9634114 + (477198.8676313 *T)
                + (0.0089970 * T * T) + ((T*T*T)/69699)
                - (T*T*T*T)/14712000 , 360);
    // moon's argument of latitude (mean distance of the moon from its ascending node)
    // Meeus P308 45.5
    double F = fmod(93.2720993 + (483202.0175273 * T)
                   - (0.0089970 * T * T) + ((T*T*T)/3526000) + ((T*T*T*T)/863310000),360);
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
    double lon = L0
    + 6.289 * sin(M)          // main term
    - 1.274 * sin(2*D - M)
    + 0.658 * sin(2*D)
    - 0.214 * sin(2*M)
    - 0.110 * sin(D);

    double lat = 5.128 * sin(F)
    + 0.280 * sin(M + F)
    + 0.277 * sin(M - F)
    + 0.173 * sin(2*D - F);

    double eps = 23.439291 - 0.0130042 * T; // obliquity of the ecliptic
    eps = eps*M_PI/180.0;
    lon = lon*M_PI/180.0;
    lat = lat*M_PI/180.0;

    double x = cos(lon) * cos(lat);
    double y = sin(lon) * cos(lat) * cos(eps) - sin(lat) * sin(eps);
    double z = sin(lon) * cos(lat) * sin(eps) + sin(lat) * cos(eps);

    double RA  = atan2(y, x);       // radians
    double Dec = asin(z);

    double d = jd - 2451545.0;
    double GMST = fmod(280.46061837 + 360.98564736629 * d, 360.0);
    if (GMST < 0) GMST += 360.0;

    double lon_sublunar = (GMST*M_PI/180.0) - RA;  // Earth-fixed longitude
    if (lon_sublunar >  M_PI) lon_sublunar -= 2*M_PI;
    if (lon_sublunar < -M_PI) lon_sublunar += 2*M_PI;

    double lat_sublunar = Dec;


    result.latitude = lat_sublunar*180.0/M_PI;
    result.longitude = lon_sublunar*180.0/M_PI;
    return (result);
}

SDL_Surface* moon_image = nullptr;
void lunar_module(ScreenFrame& panel) {
    if (!moon_image) {
        moon_image = IMG_Load("images/PIA14011.jpg");
        SDL_SetSurfaceColorKey(moon_image, 1, 0);
        icon_bin.set_dynamic(panel.GetRenderer(), moon_image, map_icons::ICON_MOON);
    }
    double phase_angle = moon_phase_angle(time(NULL));

    struct map_pin moon_pin;
    moon_pin.owner=MOD_LUNAR;
    struct GeoCoord sublunar_point = sublunar(time(NULL));
    moon_pin.lat=sublunar_point.latitude;
    moon_pin.lon=sublunar_point.longitude;
//    moon_pin.lat = 10;
//    moon_pin.lon = 10;
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