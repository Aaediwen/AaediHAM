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
