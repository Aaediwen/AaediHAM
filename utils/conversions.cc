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


// convert DOY to MM-DD

bool isLeapYear(int year) {
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

void doy_to_mmdd(const int year, uint16_t doy, int* mm, int* dd) {
    static uint16_t months[14]={0, 31, 28,  31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0};
    if ((!mm) || (!dd)) {
//        std::cout << "Invalid result locations\n";
        return;
    }
    if (doy > 366) {
//        std::cout << "Invalid DOY\n";
        return;
    }
    bool isleap = isLeapYear(year);
    *mm=0;
    *dd=0;
    for (int x = 1 ; x < 13 ; x++) {
        uint16_t monthdays = months[x];
        if (isleap && x==2) {
            monthdays++;
        }
        if (doy >0) {
            if (doy > monthdays) {
                doy -= monthdays;
            } else {
                *mm=x;
                *dd=doy;
                return;
            }
        }
    }
    return;
}

// convert YYYY MM DD HH mm ss.ss to Julian Date according to Meeus Chapter 7
double meeusJD (int year, int month, int day, int hour, int min, double sec) {
//    std::cout << "Meeus Input: " << year << "-" << month << "-" << day << "   " <<hour << ":" << min << ":" << sec << "\n";
    double result = 0;
    if ((month <3) && month > 0) {
        year--;
        month += 12;
    }
    int A, B;
    A = static_cast<int>(year/100);     // (MEEUS P61)
    B = 2 - A + static_cast<int>(A/4);  // (MEEUS P61)
    result = static_cast<int>(365.25*(year+4716)) ;//(MEEUS P61 7.1) // per Meeus, 4716 here avoids issues with negative years
    result += static_cast<int>(30.6001*(month+1)) ;// I break this across multiple lines // per Meeus, 30.6001 resolves precision issues with 30.6. 30.61 can work
    result += day ;                                // to hopefully make the sections of
    result += B ;                                  // it easier to see and understand
    result -= 1524.5;
    double day_seconds = 0;
    day_seconds += hour      *       3600;
    day_seconds += min       *       60;
    day_seconds += sec;
    result += (day_seconds/86400.0);
//    printf("Meeus Result: %.10f\n", result);
    return result;
}


/*struct GeoCoord loc_to_geo (const std::string locator) {
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
         result.longitude += (working - 'A')/12.0;

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
         result.latitude += (working - 'A')/24.0;

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
*/