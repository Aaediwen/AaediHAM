#include "celestials.h"


struct celest_coords g_celestials;              // sun and moon state

double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg) {
    //Converts latitude and solar declination from degrees to radians
    if (lon_deg < -180.0) {
        lon_deg = -180.0;
    }
    if (lon_deg > 180.0) {
        lon_deg = 180.0;
    }
    if (lat_deg < -90.0) {
        lat_deg = -90.0;
    }
    if (lat_deg > 90.0) {
        lat_deg = 90.0;
    }
    if (decl_deg < -24.0) {
        decl_deg = -24.0;
    }
    if (decl_deg > 24.0) {
        decl_deg = 24.0;
    }
    if (!utc || utc->tm_hour < 0 || utc->tm_hour > 24 || utc->tm_min < 0 || utc->tm_min > 60 || utc->tm_sec < 0 || utc->tm_sec > 60) {
        return 0;
    }
    double lat = lat_deg * M_PI / 180.0;
    double decl = decl_deg * M_PI / 180.0;

    double utc_hours = utc->tm_hour + utc->tm_min / 60.0 + utc->tm_sec / 3600.0;
    double solar_time = utc_hours + (lon_deg / 15.0);  // Local solar time for pixel
    double hour_angle = (15.0 * (solar_time - 12.0)) * M_PI / 180.0;
    double sin_alt = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hour_angle);
    if (sin_alt < -1) {
        sin_alt = -1;
    }
    if (sin_alt > 1) {
        sin_alt = 1;
    }
    return asin(sin_alt) * 180.0 / M_PI;
}

struct GeoCoord subsolar (const time_t now) {
//https://archive.org/details/astronomicalalgorithmsjeanmeeus1991/page/n155/mode/2up
//Jean Meesus Astronomical Algorithms Ch 24 (1991)

    struct GeoCoord result;
    result.longitude = 0.0;
    result.latitude = 0.0;

    // convert to Julian Centuries since J2000 (January 2000)
    // divide Unix Time by seconds per day(86400), and adjust offset to 01-01-1970 (2440587.5)
    double jd = (now / 86400.0) + 2440587.5;
    // adjust again to January 2000 and divide by 36525 days/Julian century (MESSUS P 151 24.1) T
    double T = (jd - 2451545.0) / 36525.0;
    // error of 0.00001 in T == 0.37 days

    // Sun mean anomaly (deg) (MEESUS P151 24.3) M
    double M = fmod(357.52910 + (35999.05030*T) - (0.0001559*T*T) - (0.00000048*T*T*T), 360.0);
    double M_rad = M * M_PI / 180.0;

    // Solar Equation of Center C (MEESUS P152)
    // ChatGPT gave slightly different values:
//         double C = (1.914602 - 0.004817*T - 0.000014*T*T)*sin(M*M_PI/180.0)
//             + (0.019993 - 0.000101*T)*sin(2*M*M_PI/180.0)
//             + 0.000289*sin(3*M*M_PI/180.0);
    // here we use values per Meesus
    double C = (1.914600 - (0.004817*T) - (0.000014*T*T)) * sin(M_rad)
                + (0.019993 - (0.000101*T)) * sin(2*M_rad)
                + 0.000290 * sin(3*M_rad);

    // Sun mean longitude (deg) (MEESUS P151 24.2) L0
    // per ScienceDirect article 2.1, needs to be in range 0 - 360, hence fmod 360.0
    double L0 = fmod(280.46645 + 36000.76983*T + 0.0003032*T*T, 360.0);
    // sun's true geometric Longitude and anomaly (MEESUS P152)
    double corrected_mean_solar_lon = L0 + C;
//    double corrected_mean_solar_lon_rad = corrected_mean_solar_lon * M_PI/180.0;
//    double corrected_mean_anomaly   = M + C;

    // calculate apparent longitude (MEESUS P152)
    double omega = 125.04 - 1934.136 * T;
    double apparent_longitude = corrected_mean_solar_lon - 0.00569 - 0.00478 * sin(omega * M_PI/180);
    double apparent_longitude_rad = apparent_longitude * M_PI/180.0;
    // obliquity of the eleptic per MEESUS 21.2
    // deg + min/60 + sec/3600
    double obliquity = (23.0 + (26.0/60.0) + (21.448/3600.0) )
                    - ((46.8150/3600.0) * T)
                    - ((0.00059/3600.0) * T * T)
                    + ((0.001813/3600.0) * T * T * T);
    double obliquity_rad = obliquity * M_PI / 180.0;
    // solar latitude (MEESUS P153)
    // meesus 24.6 + see note after 24.8
    double right_ascension_rad = atan2(cos(obliquity_rad) * sin(apparent_longitude_rad), cos(apparent_longitude_rad));
//     double right_ascension = cot((cos(obliquity_rad) * sin(corrected_mean_solar_lon_rad))
//                              / cos(corrected_mean_solar_lon_rad));
    double right_ascension = right_ascension_rad * (180.0/M_PI);
    if(right_ascension < 0) right_ascension += 360.0;  // normalize to [0,360)
    // meesus 24.7
    double declination_rad = asin(sin(obliquity_rad) * sin(apparent_longitude_rad));
    double declination = declination_rad * (180.0/M_PI);

    // adjust coordinate system from Celestial to geographical relative to Greenwich
//     Meesus P89
       // Greenwich Mean Sidereal Time (deg)
    double d = jd - 2451545.0;
    double GMST = fmod(280.46061837 + 360.98564736629*d, 360.0);

    // Subsolar longitude
    double lon = fmod(right_ascension - GMST, 360.0);
    if (lon < -180) lon += 360;
    if (lon > 180) lon -= 360;

    result.longitude = lon;
    result.latitude = declination;

    g_celestials.sun.timestamp=time(NULL);
    g_celestials.sun.Lat = result.latitude;
    g_celestials.sun.Lon = result.longitude;
    g_celestials.sun.RA  = right_ascension_rad;
    g_celestials.sun.Dec = declination_rad;
    return (result);
}



void sun_times(double lat, double lon, time_t* sunrise, time_t* sunset, double *solar_alt, time_t now) {
    if (!solar_alt || !sunrise || !sunset) {
        return;
    }
    // get sunrise and sunset times
    tm* utc = gmtime(&now);
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
    // get current solar altitude
    *solar_alt = solar_altitude(lat, lon, utc, solar_decl);
    // find the next zero crossing for sunrise if current alt <0

    double test_alt;
    test_alt = *solar_alt;
    tm* test_time;
    *sunrise = now;
    *sunset = now;
   if (test_alt < 0) { // it's night right now
        while (test_alt < 0) { // get sunrise time
            *sunrise    +=      5;
            *sunset     +=      5;
            test_time   =       gmtime(sunrise);
            test_alt    =       solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt > 0) {  // proceed to get the sunset time
            *sunset     +=      5;
            test_time   =       gmtime(sunset);
            test_alt    =       solar_altitude(lat, lon, test_time, solar_decl);
        }
    } else {            // it's day right now
       while (test_alt > 0) {   // get sunset time
            *sunrise    +=      5;
            *sunset     +=      5;
            test_time   =       gmtime(sunset);
            test_alt    =       solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt <0) {// proceed to get sunrise time
            *sunrise    +=      5;
            test_time   =       gmtime(sunrise);
            test_alt    =       solar_altitude(lat, lon, test_time, solar_decl);
        }
    }
}

