#include "celestials.h"


struct celest_coords g_celestials;              // sun and moon state

struct moon_ill moon_illumination;



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



struct GeoCoord sublunar(const time_t time) {
    // function to calculate the sublunar position given a unix time
    struct GeoCoord result;
    // call to get solar celestials
    result = subsolar(time);
    result = {0.0, 0.0};
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
/*    *(host_api->AaediHAM_LogDebug) << "LUNAR: Moon arguments : \nL0\t" << L0 << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: D\t " << D << "\tD_rad:\t " << D_rad << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: M\t" << M << "\tM_rad:\t" << M_rad << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: M0\t" << M0 << "\tM0_rad:\t" << M0_rad << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: F\t" << F << "\tF_rad:\t" << F_rad << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: i\t" << i << "\ti_rad:\t" << i_rad << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: illumination: "<< moon_illumination.fraction << "\n";
    *(host_api->AaediHAM_LogDebug) << "LUNAR: Time: "<< time;
*/
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
