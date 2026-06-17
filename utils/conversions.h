#include <string>
#include <map>
#include "aaediclock.h"
#include "plugins/plugin_api.h"
void cords_to_px(double lat, double lon, int w, int h, aaediclock_FPoint* result);
int month_to_int(const std::string& month);
void doy_to_mmdd(const int year, int doy, int* mm, int* dd);
double meeusJD (int year, int month, int day, int hour, int min, double sec);
struct GeoCoord loc_to_geo (const std::string locator);