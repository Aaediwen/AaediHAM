#ifndef UTILS_H
#define UTILS_H
#include <math.h>

void draw_panel_border(struct ScreenFrame panel);
double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg);

#endif
