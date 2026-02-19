#include "maidenhead.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void maidenhead(double lat, double lon, char* maiden);
struct GeoCoord loc_to_geo (const std::string locator);

void maidenhead(double lat, double lon, char* maiden) {
    if (!maiden) {
        return;
    }
    // generate maidenhead grid square
    // result should be at least 7 bytes long
    if (lon < -180.0) {
        lon = -180.0;
    }
    if (lon > 180.0) {
        lon = 180.0;
    }
    if (lat < -90.0) {
        lat = -90.0;
    }
    if (lat > 90.0) {
        lat = 90.0;
    }
   double madlon, madlat;
    madlon = lon + 180;
    madlat = lat + 90;
    maiden[6]=0;
    maiden[0]= static_cast<char>((int)(madlon/20))+65;  // Offset from 'A'
    maiden[1]= static_cast<char>((int)(madlat/10))+65;
    maiden[2]= static_cast<char>((int)(((int)madlon % 20)/2)+48);       // offset from '0'
    maiden[3]= static_cast<char>((int)(((int)madlat + 90) % 10)+48);
    maiden[4] = static_cast<char>((int)(((fmod(madlon,2.0))/2.0)*24)+97);       // offset from 'a'
    maiden[5] = static_cast<char>((int)((fmod(madlat,1.0))*24)+97);
    return;
}

struct GeoCoord loc_to_geo (const std::string locator) {
    struct GeoCoord result;
    result.latitude = 0;
    result.longitude = 0;
    if (locator.length() <4) {  //locator too short
        return result;
    }
    char working;

    // first character
    working = locator.at(0);
    if (working >= 'a') {
        working -= 32;
    }
    if (working < 'A' || working > 'R') {
        result.latitude = 0;
        result.longitude = 0;
        return result;
    }
    result.longitude += (working - 'A')*20.0;
    // second character
    working = locator.at(1);
    if (working >= 'a') {
        working -= 32;
    }
    if (working < 'A' || working > 'R') {
        result.latitude = 0;
        result.longitude = 0;
        return result;
    }
    result.latitude += (working - 'A')*10.0;
    // third character
    working = locator.at(2);
    if (working < '0' || working > '9') {
        result.latitude = 0;
        result.longitude = 0;
        return result;
    }
    result.longitude += (working - '0')*2.0;
    // fourth character
    working = locator.at(3);
    if (working < '0' || working > '9') {
        result.latitude = 0;
        result.longitude = 0;
        return result;
    }
    result.latitude += (working - '0')*1.0;
    // if we have 6, then we get those too
    if (locator.length() >= 6) {
        // fifth character
         working = locator.at(4);
         if (working >= 'a') {
             working -= 32;
         }
         if (working < 'A' || working > 'X') {
             result.latitude = 0;
             result.longitude = 0;
             return result;
         }
         result.longitude += (working - 'A')/12;

         // sixth character
         working = locator.at(5);
         if (working >= 'a') {
             working -= 32;
         }
         if (working < 'A' || working > 'X') {
             result.latitude = 0;
             result.longitude = 0;
             return result;
         }
         result.latitude += (working - 'A')/24;

         // final adjustment for 6 character
         result.longitude += (1.0/12.0);
         result.latitude  += (1.0/24.0);
         result.longitude -= 180.0;
         result.latitude  -= 90.0;
    } else {
        // final adjustment for 4 character
         result.longitude += 1.0;
         result.latitude  += 0.5;
         result.longitude -= 180.0;
         result.latitude  -= 90.0;
    }
    return result;
}
