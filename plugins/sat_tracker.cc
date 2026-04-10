#include <libsgp4/CoordTopocentric.h>
#include <mutex>
#include <vector>
#include <fstream>
#include <SDL3_image/SDL_image.h>

#include "sat_tracker.h"
#include "utils/http_fetch.h"
#include "utils/conversions.h"
//#include "aaediclock.h"
//#include "core/utils.h"
aaediclock_host_api* host_api = nullptr;
SDL_TimerID sat_timer = 0;
int fetch_result = 0;
std::mutex sat_tracker_mutex;
struct tle_cache {
    char name[80];
    char line1[80];
    char line2[80];
    aaediclock_Color color;
    bool draw;
};
std::vector<struct tle_cache>tle_raw;
uint16_t icon = 0;

TrackedSatellite::TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2): name(source_name), tle1(l1), tle2(l2) {
     sat_tle = new libsgp4::Tle(name, tle1, tle2);
     sgp4 = new libsgp4::SGP4(*sat_tle);

};

TrackedSatellite::~TrackedSatellite() {
    delete sgp4;
    delete sat_tle;
};

void TrackedSatellite::new_tracking(const std::string& source_name, const std::string& l1, const std::string& l2) {
    libsgp4::Tle* new_sat_tle = new libsgp4::Tle(source_name, l1, l2);
    libsgp4::SGP4* new_sgp4;
    try {
       new_sgp4  = new libsgp4::SGP4(*new_sat_tle);
    } catch (std::exception& e) {
        *(host_api->AaediHAM_LogDebug) << "SGP4 Exception "<< e.what() << "Regenerating " << source_name << "\n";
        delete(new_sat_tle);
        throw;
    }

    name = source_name;
    tle1=l1;
    tle2=l2;
    if (sgp4) {
        delete sgp4;
    }
    if (sat_tle) {
        delete sat_tle;
    }
    sat_tle = new_sat_tle;
    sgp4 = new_sgp4;
    telemetry.clear();

}

TrackedSatellite::TrackedSatellite(TrackedSatellite&& source) noexcept {	// move constructor -- C++11
    name = std::move(source.name);
    tle1 = std::move(source.tle1);
    tle2 = std::move(source.tle2);
    sat_tle = source.sat_tle;
    source.sat_tle=nullptr;
    sgp4 = source.sgp4;
    source.sgp4=nullptr;
    color = std::move(source.color);
    telemetry = std::move(source.telemetry);

}

TrackedSatellite& TrackedSatellite::operator=(TrackedSatellite&& source) noexcept {    // move with overwrite -- C++11
    if (this != &source) {
        name.clear();
        name = std::move(source.name);
        tle1.clear();
        tle1 = std::move(source.tle1);
        tle2.clear();
        tle2 = std::move(source.tle2);
        if (sat_tle) {
            delete (sat_tle);
        }
        if (sgp4) {
            delete (sgp4);
        }
        sat_tle = source.sat_tle;
        source.sat_tle=nullptr;
        sgp4 = source.sgp4;
        source.sgp4=nullptr;
        color = std::move(source.color);
        telemetry.clear();
        telemetry = std::move(source.telemetry);
    }
    return (*this);
}

TrackedSatellite::TrackedSatellite(const TrackedSatellite& source) {              // copy to new
    name = source.name;
    tle1 = source.tle1;
    tle2 = source.tle2;
    sat_tle = new libsgp4::Tle(name, tle1, tle2);
    sgp4 = new libsgp4::SGP4(*sat_tle);
    color = source.color;
    telemetry = source.telemetry;

}

TrackedSatellite& TrackedSatellite::operator=(const TrackedSatellite& source) { // copy with overwrite
    if (this != &source) {
        libsgp4::Tle* new_sat_tle = new libsgp4::Tle(source.name, source.tle1, source.tle2);
        libsgp4::SGP4* new_sgp4;
        try {
           new_sgp4  = new libsgp4::SGP4(*new_sat_tle);
        } catch (std::exception& e) {
            *(host_api->AaediHAM_LogDebug) << "SGP4 Exception "<< e.what() << "Assigning " << source.name << "\n";
            delete(new_sat_tle);
            throw;
        }
        name.clear();
        name = source.name;
        tle1.clear();
        tle1 = source.tle1;
        tle2.clear();
        tle2 = source.tle2;
        if (sat_tle) {
            delete (sat_tle);
        }
        if (sgp4) {
            delete (sgp4);
        }
        sat_tle = new_sat_tle;
        sgp4 = new_sgp4;
        color = source.color;
        telemetry = source.telemetry;
    }
    return (*this);
}

const std::string& TrackedSatellite::get_name() const {
    return (this->name);
}

time_t TrackedSatellite::pass_start() {
    // get the start time for a satellite pass
    if (telemetry.empty()) {
        return 0;
    }
    for (const SatTelemetry& point : this->telemetry) {
        if (point.elevation >0) {
            return point.timestamp;
        }
    }
    return 0;
}

time_t TrackedSatellite::pass_end() {
    // get the end time for a satellite pass
    if (telemetry.empty()) {
        return 0;
    }
    bool started_flag=false;
    for (const SatTelemetry& point : this->telemetry) {
        if (point.elevation >0) {
            started_flag = true;
        }
        if (started_flag && point.elevation < 0) {
            return point.timestamp;
        }
    }
    if (started_flag) {
        return (this->telemetry.back().timestamp);
    } else {
        return 0;
    }
}


void TrackedSatellite::draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<aaediclock_FPoint> *pass_pts, const aaediclock_FRect *size) {
    // render the pass line for a satellite pass

    if (!pass_pts) {
        return;
    }
    pass_pts->clear();
    if (telemetry.empty()) {
        return;
    }
    if (size->x <=2 || size->y <=2) {
        return;
    }
/*    int pass_samples=0;
    for (const SatTelemetry& point : this->telemetry) {
        if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
            pass_samples++;
        }
    }
    *(host_api->AaediHAM_LogDebug) << "Rendering "<< pass_samples << " samples for pass path of " << this->name.c_str() << "\n";
    */
    float max_radius = size->w/2;
    if (size->h < size->w) {
        max_radius = size->h/2;
    }
    aaediclock_FPoint center, new_point;
    center.x = (size->w/2)+size->x;
    center.y = (size->h/2)+size->y;
    for (const SatTelemetry& point : this->telemetry) {
        if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
            float radius = max_radius * (1- static_cast<float>(point.elevation)/90.0f);
            new_point.x = center.x + radius * sinf(static_cast<float>(point.azimuth)*(static_cast<float>(M_PI)/180.0f));
            new_point.y = center.y - radius * cosf(static_cast<float>(point.azimuth)*(static_cast<float>(M_PI)/180.0f));
//            *(host_api->AaediHAM_LogDebug) << "SAT_TRACKER: AZ: " << point.azimuth << ", EL: " << point.elevation << " Radius " << radius << "\n";
            pass_pts->push_back(new_point);
        }
    }
    return;
}

void TrackedSatellite::add_telemetry(const double lat,const double lon, const double elevation, const double azimuth, const time_t timestamp) {
    // add a telemetry node for the satellite track
    struct SatTelemetry new_node;
    new_node.lat=lat;
    new_node.lon=lon;
    new_node.elevation=elevation;
    new_node.azimuth=azimuth;
    new_node.timestamp=timestamp;
    telemetry.push_back(new_node);
    return;
}

void TrackedSatellite::location (aaediclock_FPoint *result) {
    // get the current lat/lon over which the satellite currently is
    if (!result) {
        return;
    }
    libsgp4::DateTime dt = libsgp4::DateTime::Now();
    libsgp4::Eci eci = this->sgp4->FindPosition(dt);
    libsgp4::CoordGeodetic geo = eci.ToGeodetic();
    result->x = static_cast<float>(geo.latitude) * (180.0f/ static_cast<float>(M_PI));
    result->y = static_cast<float>(geo.longitude) * (180.0f/ static_cast<float>(M_PI));
    return ;
}


bool TrackedSatellite::gen_telemetry(const int resolution, libsgp4::Observer& obs) {
    // generate the telemetry track for a satellite
    if (resolution < 1) {
        return false;
    }
    telemetry.clear();
    bool add_flag;
    add_flag=true;
    int i;
    double mean_motion =  this->sat_tle->MeanMotion();
    double period = (1440*60)/mean_motion; // period in seconds
    i = static_cast<int>(floor(period/resolution));
    const time_t nowtime = time(NULL);
    for (int offset = 0 ; offset < (i*resolution) ; offset +=resolution) {
        try {
           libsgp4::DateTime dt = libsgp4::DateTime::Now().AddSeconds(offset);
            libsgp4::Eci eci = this->sgp4->FindPosition(dt);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();
            libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);
            double lat_deg = geo.latitude * (180/M_PI);
            double long_deg = geo.longitude * (180/M_PI);
            double elevation_deg = topo.elevation * (180/M_PI);
            double azimuth_deg = topo.azimuth * (180/M_PI);
            this->add_telemetry(lat_deg, long_deg, elevation_deg, azimuth_deg, (nowtime+offset));
        } catch (const libsgp4::SatelliteException& e) {
            (void)e;
            add_flag=false;
            break;
        }
    }
    return add_flag;
}

time_t TrackedSatellite::telemetry_age() {
    if (telemetry.empty()) {
        return 0;
    } else {
        return (telemetry.back().timestamp);
    }
}




void TrackedSatellite::draw_telemetry(aaediclock_FRect& dims) {
//void TrackedSatellite::draw_telemetry(ScreenFrame& map) {

    if (dims.w < 1 || dims.h < 1) {
        return;
    }
/*    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Satellite Draw during resize event!");
        return;
    }
    // draw the satellite's telemetry track on the map
    if (this->telemetry.empty()) { return; }
    if (!map.GetRenderer()) {
        *(host_api->AaediHAM_LogDebug) << "SAT_TRACKER: Missing Renderer!\n";
        return;
    }
    if (!map.texture) {
        *(host_api->AaediHAM_LogDebug) << "SAT_TRACKER: Missing PANEL!\n";
        return;
    }
*/

    *(host_api->AaediHAM_LogDebug) << "Draw telemetry on overlay\n";
    host_api->AaediHAM_OverlaySet(dims);
//    SDL_SetRenderTarget(map.GetRenderer(), map.texture);
//    SDL_SetRenderDrawColor(map.GetRenderer(), this->color.r, this->color.g, this->color.b, this->color.a);
    aaediclock_FPoint* SDLPoints = (aaediclock_FPoint*)malloc(sizeof(SDL_FPoint)*this->telemetry.size());

    int index=0;
    int render_size=0;
    int xt, yt;
    xt = static_cast<int>(dims.w);
    yt = static_cast<int>(dims.h);
    for (SatTelemetry& point : this->telemetry) {
        // calculate the current point
        cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
        render_size++;
        if (point.elevation >0) {

//            SDL_SetRenderDrawColor(map.GetRenderer(), 0, 0, 128, 255);
            aaediclock_Color draw_color = aaediclock_Color{0,0,128,255};
            aaediclock_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 4.0, 4.0};
            host_api->AaediHAM_GraphicsDrawRect(draw_color, visirect, 1);
//            SDL_RenderFillRect(map.GetRenderer(), &visirect);
//            SDL_SetRenderDrawColor(map.GetRenderer(), this->color.r, this->color.g, this->color.b, this->color.a);
        }
        if (index >1) { // if we have a last point to compare to
            if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/4)) {      // and the delta is greater than 100
                //render the current segment
                host_api->AaediHAM_GraphicsDrawLines(this->color, SDLPoints, render_size-1);
//                SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size-1);
                //reset the index
                index=0;
                render_size=1;
                // re-gen the current pixel
                cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
            }
        }
        index++;
    }
    host_api->AaediHAM_GraphicsDrawLines(this->color, SDLPoints, render_size);
//    SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size);
    free (SDLPoints);
//    SDL_SetRenderTarget(map.GetRenderer(), NULL);
    return;
}





void circle_helper(std::vector<aaediclock_FPoint> *circle_points, float radius, aaediclock_FPoint center, int segments) {
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * static_cast<float>(M_PI) * i) / segments;
        aaediclock_FPoint pt = {
            center.x + radius * cosf(theta),
            center.y + radius * sinf(theta)
        };
        circle_points->push_back(pt);
    }
    return;
}

void pass_tracker(aaediclock_FRect dims, TrackedSatellite& sat) {
    if (dims.w < 1 || dims.h < 1) {
        return;
    }
    char tempstr[30];
    // clear the box
    host_api->AaediHAM_GraphicsClear();
//    panel.Clear();
    aaediclock_FRect TextRect;
    TextRect.w=dims.w/2;
    TextRect.h=dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    // render the satellite name

//    sprintf(tempstr, "%s", sat.get_name().c_str());
//    panel.render_text(TextRect, Sans, sat.color, tempstr);
    host_api->AaediHAM_GraphicsDrawText(sat.get_name().c_str(), sat.color, TextRect);
//    panel.render_text(TextRect, Sans, sat.color, sat.get_name().c_str());
    // if we have a pass coming, render its start and end times
    time_t pass_time = sat.pass_start();
    if (pass_time) {
        tm* test_time;
        TextRect.y=dims.h - (dims.h/11)-4;
        TextRect.w=dims.w /3;
        test_time = localtime(&pass_time);
        strftime(tempstr, 12, "%H:%M", test_time);
        host_api->AaediHAM_GraphicsDrawText(tempstr, sat.color, TextRect);

//        panel.render_text(TextRect, Sans, sat.color, tempstr);
        TextRect.x=dims.w - (dims.w/3);
        pass_time = sat.pass_end();
        test_time = localtime(&pass_time);
        strftime(tempstr, 12, "%H:%M", test_time);
        host_api->AaediHAM_GraphicsDrawText(tempstr, sat.color, TextRect);
//        panel.render_text(TextRect, Sans, sat.color, tempstr);
    }

    // render the crosshairs and target pass chart
    host_api->AaediHAM_SetTarget();
//    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    std::vector<aaediclock_FPoint> circle_pts;
    float radius = dims.w/2;
    if (dims.h < dims.w) {
        radius = dims.h/2;
    }
    radius *= 0.8f;
    std::vector<aaediclock_FPoint> pass_pts;
    aaediclock_FRect pass_box;
    pass_box.x=(dims.w - (2*radius))/2;
    pass_box.y=(dims.h - (2*radius))/2;
    pass_box.w=2*radius;
    pass_box.h=2*radius;

    // cross hairs
    aaediclock_Color draw_color = aaediclock_Color{64, 64, 0 ,255};
    aaediclock_FRect line;
    aaediclock_FPoint center = aaediclock_FPoint{dims.w/2, dims.h/2};
    line = aaediclock_FRect{center.x+radius, center.y, center.x-radius, center.y};
    line.x = center.x-radius;
    line.y = center.y;
    line.w = center.x+radius;
    line.h = center.y;
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
    line.x = center.x;
    line.y = center.y-radius;
    line.w = center.x;
    line.h = center.y+radius;
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
/*
//    SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 64, 0, 255);
    line = aaediclock_FRect{center.x, center.y, center.x-radius, center.y};
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
//    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x-radius, center.y);
    line = aaediclock_FRect{center.x, center.y, center.x+radius, center.y};
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
//    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x+radius, center.y);
    line = aaediclock_FRect{center.x, center.y, center.x, center.y+radius};
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
//    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x, center.y+radius);
    line = aaediclock_FRect{center.x, center.y, center.x, center.y-radius};
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
//    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x, center.y-radius);
*/
    // concentric degree circles
//    SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 0, 64, 255);
    draw_color = aaediclock_Color{64, 0, 64 ,255};
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
//    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
//    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
//    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius *=3;
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
//    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
//    SDL_RenderPoint(panel.GetRenderer(), center.x, center.y);

    // draw the pass trajectory
    sat.draw_pass(sat.pass_start(), sat.pass_end(),  &pass_pts, &pass_box);
    draw_color = aaediclock_Color{0, 128, 0, 255};
//    SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 128, 0, 255);
    host_api->AaediHAM_GraphicsDrawLines(draw_color, pass_pts.data(), static_cast<int>(pass_pts.size()));
//    SDL_RenderLines(panel.GetRenderer(), pass_pts.data(), static_cast<int>(pass_pts.size()));
    return;
}

void sat_json_parser(const char* input_string) {
    if (!input_string || ! input_string[0]) {
        return;
    }
    std::istringstream iostring_buffer;
    std::string sanitized(input_string);  // Make a copy (if amateur_tle is char*)
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\r'), sanitized.end());
    iostring_buffer.clear();
    iostring_buffer.str(sanitized);
    aaediclock_Color trackcols = {255,0,0,255};
    const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
    tle_raw.clear();
    while (true) {
        struct tle_cache new_cache;
        memset(&new_cache, 0, sizeof(new_cache));
        std::string instring;
        std::getline(iostring_buffer, instring);
        strncpy(new_cache.name, instring.c_str(),80);
        new_cache.name[79]=0;
        std::getline(iostring_buffer, instring);
        strncpy(new_cache.line1, instring.c_str(),80);
        new_cache.line1[79]=0;
        std::getline(iostring_buffer, instring);
        strncpy(new_cache.line2, instring.c_str(),80);
        new_cache.line2[79]=0;
        if (new_cache.name[0] && new_cache.line1[0] && new_cache.line2[0]) { // valid entry?

            new_cache.draw=false;
            trackcols.r -= 20;
            trackcols.g += 20;
            trackcols.b += 10;
            *(host_api->AaediHAM_LogDebug) << "Processing: " << new_cache.name << " ";
            int sat_count = host_api->AaediHAM_ConfigGetSatCount();
            for (int x = 0 ; x < sat_count ; x++) {
                instring = new_cache.name;
                const std::string stropt = host_api->AaediHAM_ConfigGetSat(x);
                if (instring.compare(0,stropt.length(),stropt)==0) {
        	    new_cache.draw=true;
                    new_cache.color = trackcols;
                }
            }

            if (new_cache.draw) {
                *(host_api->AaediHAM_LogDebug) << "Setting to draw \n";
            } else {
                *(host_api->AaediHAM_LogDebug) << "\n";
            }
            if (new_cache.draw) {
                tle_raw.push_back(new_cache);
            }
        } else { break; }
    }
    return ;
}

Uint16 pass_pager[2] = {0,0};
std::vector<TrackedSatellite> satlist;
int SDLCALL fetch_celestrak(void* data) {
    (void) data;

    char* amateur_tle = 0 ;
    Uint64 data_size = 0;
    SDL_PathInfo fileinfo;
    bool file_valid = false;
    std::fstream disk_file;
    if (SDL_GetPathInfo("celestrak.cache", &fileinfo)) {
        SDL_Time sdl_now;
        SDL_GetCurrentTime(&sdl_now);
        if ((sdl_now - fileinfo.modify_time) < 10800000000000  ) { // 3 Hours in ns
            data_size = fileinfo.size;
            disk_file.open("celestrak.cache", (std::fstream::binary | std::fstream::in ));
            if (disk_file.is_open()) {
                amateur_tle = (char*)malloc(fileinfo.size+1);
                if (amateur_tle) {
                    if (disk_file.read(amateur_tle, fileinfo.size)) {
                        amateur_tle[fileinfo.size] = '\0';
                        file_valid = true;
                    } else {
                        free(amateur_tle);
                        amateur_tle = 0;
                    }
                }
                disk_file.close();
            }
        }
    }
    if (!file_valid) {
        SDL_Log ("Fetching Satellite telemetry from Celestrak");
        *(host_api->AaediHAM_LogDebug) << "Fetching Satellite telemetry from Celestrak\n";
        data_size = http_loader("https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=tle", (void**)&amateur_tle);   //
        *(host_api->AaediHAM_LogDebug) << "Celestrak Fetch returned\n";
        if (data_size > 256) {
            disk_file.open("celestrak.cache", (std::fstream::binary | std::fstream::out | std::fstream::trunc));
            if (disk_file.is_open()) {
                disk_file.write(amateur_tle, data_size);
                if (!disk_file.good()) {
                     *(host_api->AaediHAM_LogDebug) << "Cache write failed\n";

                }
            }
            disk_file.close();
        }
    }
    satlist.clear();
    if (data_size) {
        *(host_api->AaediHAM_LogDebug) << "Fetched New Sat data\n";
        satlist.clear();
        sat_json_parser(amateur_tle);

        if (amateur_tle) {
            free(amateur_tle);
            amateur_tle=0;
        }
        fetch_result = 2;
    } // we got input data
    else {
        *(host_api->AaediHAM_LogDebug) << "No New Sat data from Celestrak\n";
        fetch_result = 3;
    }
    *(host_api->AaediHAM_LogDebug) << "Fetch returned "<< data_size <<" bytes\n";
    return 0;
}

Uint32 SDLCALL fetch_celestrak (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)userdata;
    (void)interval;
     if (timerID) {
         const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
         fetch_result = 10;
         SDL_Thread* thread = SDL_CreateThread(fetch_celestrak, "Celestrak Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              *(host_api->AaediHAM_LogDebug) << "Failed to Create Sat Data Fetch Thread\n";
          }

     }
     sat_timer = 0;
     return 0;
}



extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new sat_tracker_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void sat_tracker_plugin::plugin_init() const {
    return;
}

void sat_tracker_plugin::plugin_exit() const {
    if (sat_timer) {
        SDL_RemoveTimer(sat_timer);
    }
    if (host_api->AaediHAM_IconCheck(icon)) {
        host_api->AaediHAM_IconDelete(icon);
    }
    return;
}

void sat_tracker_plugin::plugin_main(const aaediclock_FRect& dims) const {
    if (!sat_timer) {
        const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
        Uint32 interval;
        interval = 3600000 ;

        switch (fetch_result) {
            case 0:
                sat_timer = SDL_AddTimer(10000, fetch_celestrak, NULL);
                break;
            case 10:
                break;
            case 2:
                sat_timer = SDL_AddTimer(interval * 6, fetch_celestrak, NULL);
                break;
            case 3:
                sat_timer = SDL_AddTimer(interval * 2, fetch_celestrak, NULL);
                break;
        }
    }

    aaediclock_FRect TextRect;
    host_api->AaediHAM_MapPinDelete();


    // clear the box
    host_api->AaediHAM_GraphicsClear();
    aaediclock_FRect mapsize = host_api->AaediHAM_GetMapSize();
    bool redraw_flag = false;
    redraw_flag = (!host_api->AaediHAM_OverlayCheck());

    // render the header
    TextRect.w=dims.w/2-10;
    TextRect.h=dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    host_api->AaediHAM_GraphicsDrawText("SAT TRACKERS", aaediclock_Color{128,128,0,255}, TextRect);
    TextRect.w=dims.w-10;
    TextRect.y += ((dims.h/11)+(dims.h/150));

    if (!tle_raw.empty()) {
        *(host_api->AaediHAM_LogDebug) << "We have tracking data: "<< tle_raw.size() << " Entries";
    } else {
        *(host_api->AaediHAM_LogDebug) <<"Tracking Data Fetch Error!\n";
        TextRect.w=dims.w-10;
        TextRect.h=dims.h/11;
        TextRect.x=5;
        TextRect.y=dims.h/10;
        if (TextRect.w > 5) {
            host_api->AaediHAM_GraphicsDrawText("NO SAT DATA", aaediclock_Color{128,128,0,255}, TextRect);
        }
        return;
    }
    struct aaediclock_dx de_loc = host_api->AaediHAM_ConfigGetDE();
    libsgp4::Observer obs(de_loc.lat, de_loc.lon, 0.27); // need a way to manage altitude here (last arg)
    // read the TLE data from Celestrak
    *(host_api->AaediHAM_LogDebug) << "Reading Sat lists from Celestrak\n";
//    struct tle_cache temp;
    for (struct tle_cache& temp  : tle_raw) {
//    while (tle_raw.read(reinterpret_cast<char*>(&temp), sizeof(temp))) {
        temp.name[49]=0;
        temp.line1[69]=0;
        temp.line2[69]=0;
        // read the TLE for a sat
        // is it one we want to show?
        *(host_api->AaediHAM_LogDebug) << "Read Sat " << temp.name << " with draw=" << temp.draw << "\n";
        bool draw_flag=temp.draw;
        // check if the sat exists in satlist
        TrackedSatellite *nextsat = nullptr;
        if (draw_flag) {

            std::string name(temp.name);
            std::string line1(temp.line1);
            std::string line2(temp.line2);
            const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
            for (TrackedSatellite& sat : satlist) {
                if (name.compare(0,sat.get_name().length(),sat.get_name())==0) {
                    nextsat = &sat;
                }
            }

            if (nextsat) {
                *(host_api->AaediHAM_LogDebug) << "Found NextSat: " << temp.name << "\n";
                if (redraw_flag) {
                    nextsat->new_tracking(name, line1, line2);
                    nextsat->gen_telemetry(30, obs);
                    redraw_flag = true;
                }
            } else {
                *(host_api->AaediHAM_LogDebug) << "Creating New Sat entry with:\n "
                          << temp.name << "\n"
                          << temp.line1 << "\n"
                          << temp.line2 << "\n";
                try {
                    nextsat = new TrackedSatellite(temp.name, temp.line1, temp.line2);
                    nextsat->color=temp.color;
                    *(host_api->AaediHAM_LogDebug) << "Regenerate track for " << temp.name << "\n";
                    if (nextsat->gen_telemetry(30, obs)) {
                        satlist.push_back(std::move(*nextsat));
                        delete (nextsat);
                        nextsat = nullptr;
                        redraw_flag= true;
                    }
                } catch (const std::exception& e){
                    SDL_Log ("Failed to create Satellite tracking entry for %s\n%s", temp.name, e.what());
                     *(host_api->AaediHAM_LogDebug) << "Failed to create Satellite tracking entry for " << temp.name
                              << "\n" << e.what() << "\n";
                }
            }
        }
    } // read from Celestrak
    *(host_api->AaediHAM_LogDebug) << "Displaying Selected Satellites\n";
    // display the selected satellites
    const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
    if (!satlist.empty()){
        if (pass_pager[0] >= satlist.size()) {
            pass_pager[0]=0;
        }
        pass_tracker(dims, satlist[pass_pager[0]]);
        if (pass_pager[1] >5) {
            pass_pager[0]++;
            pass_pager[1]=0;
        }
        pass_pager[1]++;
        for (TrackedSatellite& Sat : satlist) {
            const time_t time_now = time(NULL);
            if ((Sat.telemetry_age() - time_now) < 60) {
                Sat.gen_telemetry(30, obs);
                redraw_flag = true;
            }
        }
        host_api->AaediHAM_OverlaySet(mapsize);
        if (redraw_flag) {
            host_api->AaediHAM_OverlayClear(aaediclock_Color{0,0,0,0});
        }
        for (TrackedSatellite& Sat : satlist) {
            if (redraw_flag) {
                *(host_api->AaediHAM_LogDebug) << "Redrawing track for " << Sat.get_name().c_str() << "\n";
                Sat.draw_telemetry(mapsize);
            }
            // plot the sat's current location
            struct aaediclock_map_pin sat_pin;
            aaediclock_FPoint sat_loc;
            Sat.location(&sat_loc);
            sat_pin.owner   =               0;
            sprintf(sat_pin.label, "%s", Sat.get_name().c_str());
            sat_pin.lat     =               sat_loc.x;
            sat_pin.lon     =               sat_loc.y;
            sat_pin.icon = 0;
            if (!host_api->AaediHAM_IconCheck(icon)) {
                SDL_Surface* loadsurface = IMG_Load("images/satellite.png");
                if (loadsurface) {
                    aaediclock_image new_icon;
                    new_icon.width = loadsurface->w;
                    new_icon.height = loadsurface->h;
                    new_icon.pixels = static_cast<uint8_t*>(loadsurface->pixels);
                    icon = host_api->AaediHAM_IconCreate(new_icon);
                    SDL_DestroySurface(loadsurface);
                }
            }
            sat_pin.icon  	=	icon;
//
//            sat_pin.icon    =               icon_bin.get_icon(map_icons::ICON_SAT);
            *(host_api->AaediHAM_LogDebug) << "got pin " <<  sat_pin.icon << "\n";
            sat_pin.color   =               Sat.color;
            sat_pin.tooltip[0]      =               0;
            host_api->AaediHAM_MapPinAdd(sat_pin);
        }

        *(host_api->AaediHAM_LogDebug) << "Loaded "<< satlist.size() << " SATS\n";
    } else {
        host_api->AaediHAM_GraphicsDrawText("NO SELECTED SATS", aaediclock_Color{128,128,0,255}, aaediclock_FRect {dims.w/20, dims.h/4, (dims.w/10)*8, dims.h/10});
    }

}

const char* sat_tracker_plugin::getName() const {
    return "Sat Tracker Plugin";
}

void sat_tracker_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

