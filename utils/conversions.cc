#include "conversions.h"
void cords_to_px(double lat, double lon, int w, int h, aaediclock_FPoint* result) {
    if (!result) return;
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
    result->x=static_cast<float>((lon/180.0f)*(w/2.0f)+(w/2.0f));
    result->y= static_cast<float>(((-1*lat)/90.0f)*(h/2.0f)+(h/2.0f));
    return ;
}

int month_to_int(const std::string& month) {
     static const std::map<std::string, int> months = {
                                                         {"Jan", 0}, {"Feb", 1}, {"Mar", 2}, {"Apr", 3},
                                                         {"May", 4}, {"Jun", 5}, {"Jul", 6}, {"Aug", 7},
                                                         {"Sep", 8}, {"Oct", 9}, {"Nov", 10}, {"Dec", 11}
                                                    };
    auto temp = months.find(month.substr(0,3));
    if (temp != months.end()) {
        return temp->second;
    } else {
        return -1;
    }
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
