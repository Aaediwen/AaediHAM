#include "aaediclock.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <ctime>

struct moon_ill{
    time_t timestamp    = 0;
    double fraction     = 0.0;
    double angle        = 0.0;
    double i            = 0.0;
} extern moon_illumination;

namespace LunarConstants {
    constexpr double JD_UNIX_EPOCH              = 2440587.5;    // offset to 01-01-1970 (2440587.5)
    constexpr double SECONDS_PER_DAY            = 86400.0;      // seconds per calendar day
    constexpr double DAYS_PER_JULIAN_CENTURY    = 36525.0;      // days per Julian Century
    constexpr double SYNODIC_MONTH              = 29.53058867;  // synodic month
    constexpr double J2000                      = 2451545.0;    // Julian Date of J2000.0, which is January 1, 2000 at 12:00 TT
    constexpr double NEW_MOON                   = 2451550.1;    // Julian Date of known new moon (Jan 6, 2000 18:14 UT)
    constexpr double OBLIQUITY_J2000            = 23.4392911;   // (23.0 + (26.0/60.0) + (21.448/3600.0) )  23°26'21.448"
}


struct Celestial_Coordinates {
        time_t timestamp= 0;
        double RA       = 0.0;
        double Dec      = 0.0;
        double Lon      = 0.0;
        double Lat      = 0.0;
        double Dist     = 0.0;
};
struct celest_coords {
    struct Celestial_Coordinates moon;
    struct Celestial_Coordinates sun;
};
extern struct celest_coords g_celestials;

double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg);
struct GeoCoord subsolar(const time_t now);

void sun_times(double lat, double lon, time_t* sunrise, time_t* sunset, double *solar_alt, time_t now);

struct GeoCoord sublunar(const time_t time);