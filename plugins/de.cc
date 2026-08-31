#include "de.h"
#include "utils/maidenhead.h"
#include "utils/celestials.h"

aaediclock_host_api* host_api = nullptr;
extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new de_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void de_plugin::plugin_init() const {
    return;
}

void de_plugin::plugin_exit() const {
    return;
}

void de_plugin::plugin_main(const aaediclock_FRect& dims) const {

    char tempstr[64];
    aaediclock_FRect TextRect;
    aaediclock_Color fontcolor;
    struct aaediclock_dx location;
    fontcolor.r=128;
    fontcolor.g=255;
    fontcolor.b=128;
    fontcolor.a=0;
    // get DE
    location = host_api->AaediHAM_ConfigGetDE();

    // blank the box

    host_api->AaediHAM_GraphicsClear();


    time_t sunrise;
    time_t sunset;
    double solar_alt;
    // find the next zero crossing for sunrise if current alt <0

    sun_times(location.lat, location.lon, &sunrise, &sunset, &solar_alt, time(NULL));
    // render the header
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(dims.h)/4;
    TextRect.w=(dims.w)-4;
    struct aaediclock_map_pin de_pin;
    host_api->AaediHAM_GraphicsDrawText("DE:", fontcolor, TextRect);
    de_pin.owner=0;
    sprintf(de_pin.label, "DE");
    struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
    if (mouse_event.valid) {
//        std::cout << "Click event in DE/DX module at " << mouse_event.coords.x << ", "<< mouse_event.coords.y << "\n";
	*(host_api->AaediHAM_LogUser)  << "Click event in DE/DX module at " << mouse_event.coords.x << ", "<< mouse_event.coords.y << "\n";

    }
    de_pin.lat=location.lat;
    de_pin.lon=location.lon;
    de_pin.icon=0;
    de_pin.color=fontcolor;
    de_pin.color.a=255;
    de_pin.tooltip[0]=0;
    host_api->AaediHAM_MapPinDelete();
    host_api->AaediHAM_MapPinAdd(de_pin);

    // generate maidenhead grid square

    char latstr[2];
    char lonstr[2];
    char maiden[7];
    maidenhead(location.lat, location.lon, maiden);
    // lat/lon string generation
    if (location.lat < 0) {
        location.lat *= -1;
        latstr[0]='S';
        latstr[1]=0;
    } else {
        latstr[0]='N';
        latstr[1]=0;
    }

    if (location.lon < 0) {
        location.lon *= -1;
        lonstr[0]='W';
        lonstr[1]=0;
    } else {
        lonstr[0]='E';
        lonstr[1]=0;
    }
    sprintf(tempstr, "%2.2f%s %2.2f%s", location.lat, latstr, location.lon, lonstr);
    // render lat/lon
    TextRect.x=2;
    TextRect.y=(dims.h)/4;
    TextRect.h=(dims.h)/4;
    TextRect.w=dims.w-4;
    host_api->AaediHAM_GraphicsDrawText(tempstr, fontcolor, TextRect);



    // render maidenhead
    TextRect.x=2;
    TextRect.y=(dims.h)/2;
    TextRect.h=(dims.h)/4;
    TextRect.w=dims.w-4;
    sprintf(tempstr, "%s", maiden);
    host_api->AaediHAM_GraphicsDrawText(tempstr, fontcolor, TextRect);

    // render sunrise time
    TextRect.x=2;
    TextRect.y=((dims.h)/4)*3;
    TextRect.h=(dims.h)/8;
    TextRect.w=(dims.w/3)-4;
    tm* test_time = localtime(&sunrise);
    strftime(tempstr, 12, "R%H:%M", test_time);
    host_api->AaediHAM_GraphicsDrawText(tempstr, fontcolor, TextRect);

    // render solar angle
    TextRect.x=(dims.w/3)+8;
    TextRect.y=((dims.h)/4)*3;
    TextRect.h=(dims.h)/10;
    TextRect.w=(dims.w/3)-16;
    test_time = localtime(&sunset);
    sprintf (tempstr, "%2.2f", solar_alt);
    host_api->AaediHAM_GraphicsDrawText(tempstr, fontcolor, TextRect);

    // render sunset time
    TextRect.x=(dims.w/3)*2;
    TextRect.y=((dims.h)/4)*3;
    TextRect.h=(dims.h)/8;
    TextRect.w=(dims.w/3)-4;
    test_time = localtime(&sunset);
    strftime(tempstr, 12, "S%H:%M", test_time);
    host_api->AaediHAM_GraphicsDrawText(tempstr, fontcolor, TextRect);

}

const char* de_plugin::getName() const {
    return "DE Module";
}

void de_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

