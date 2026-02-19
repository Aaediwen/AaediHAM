#include "aaediclock.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <ctime>

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

