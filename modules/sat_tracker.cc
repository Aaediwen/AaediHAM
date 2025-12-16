#include <libsgp4/CoordTopocentric.h>

#include "sat_tracker.h"
#include "../aaediclock.h"
#include "../utils.h"

SDL_TimerID sat_timer = 0;
int fetch_result = 0;

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
        debug_log << "SAT: SGP4 Exception "<< e.what() << "Regenerating " << source_name << "\n";
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
            debug_log << "SAT: SGP4 Exception "<< e.what() << "Assigning " << source.name << "\n";
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


void TrackedSatellite::draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<SDL_FPoint> *pass_pts, const SDL_FRect *size) {
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
    debug_log << "SAT TRACKER: Rendering "<< pass_samples << " samples for pass path of " << this->name.c_str() << "\n";
    */
    float max_radius = size->w/2;
    if (size->h < size->w) {
        max_radius = size->h/2;
    }
    SDL_FPoint center, new_point;
    center.x = (size->w/2)+size->x;
    center.y = (size->h/2)+size->y;
    for (const SatTelemetry& point : this->telemetry) {
        if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
            float radius = max_radius * (1- static_cast<float>(point.elevation)/90.0f);
            new_point.x = center.x + radius * sinf(static_cast<float>(point.azimuth)*(static_cast<float>(M_PI)/180.0f));
            new_point.y = center.y - radius * cosf(static_cast<float>(point.azimuth)*(static_cast<float>(M_PI)/180.0f));
//            debug_log << "SAT_TRACKER: AZ: " << point.azimuth << ", EL: " << point.elevation << " Radius " << radius << "\n";
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

void TrackedSatellite::location (SDL_FPoint *result) {
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





void TrackedSatellite::draw_telemetry(ScreenFrame& map) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Satellite Draw during resize event!");
        return;
    }
    // draw the satellite's telemetry track on the map
    if (this->telemetry.empty()) { return; }
    if (!map.GetRenderer()) {
        debug_log << "SAT: Missing Renderer!\n";
        return;
    }
    if (!map.texture) {
        debug_log << "SAT: Missing PANEL!\n";
        return;
    }


    debug_log << "SAT TRACKER: Draw telemetry on texture: " << (void*)map.texture << "\n";
    SDL_SetRenderTarget(map.GetRenderer(), map.texture);
    SDL_SetRenderDrawColor(map.GetRenderer(), this->color.r, this->color.g, this->color.b, this->color.a);
    SDL_FPoint* SDLPoints = (SDL_FPoint*)malloc(sizeof(SDL_FPoint)*this->telemetry.size());

    int index=0;
    int render_size=0;
    int xt, yt;
    xt = static_cast<int>(map.dims.w);
    yt = static_cast<int>(map.dims.h);
    for (SatTelemetry& point : this->telemetry) {
        // calculate the current point
        cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
        render_size++;
        if (point.elevation >0) {
            SDL_SetRenderDrawColor(map.GetRenderer(), 0, 0, 128, 255);
            SDL_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 4.0, 4.0};
            SDL_RenderFillRect(map.GetRenderer(), &visirect);
            SDL_SetRenderDrawColor(map.GetRenderer(), this->color.r, this->color.g, this->color.b, this->color.a);
        }
        if (index >1) { // if we have a last point to compare to
            if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/4)) {      // and the delta is greater than 100
                //render the current segment
                SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size-1);
                //reset the index
                index=0;
                render_size=1;
                // re-gen the current pixel
                cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
            }
        }
        index++;
    }

    SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size);
    free (SDLPoints);
    SDL_SetRenderTarget(map.GetRenderer(), NULL);
    return;
}





void circle_helper(std::vector<SDL_FPoint> *circle_points, float radius, SDL_FPoint center, int segments) {
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * static_cast<float>(M_PI) * i) / segments;
        SDL_FPoint pt = {
            center.x + radius * cosf(theta),
            center.y + radius * sinf(theta)
        };
        circle_points->push_back(pt);
    }
    return;
}

void pass_tracker(ScreenFrame& panel, TrackedSatellite& sat) {

    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Sat tracker during resize event!");
        return;
    }
        if (!Sans) {
        debug_log << "SAT: No font defined\n";
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "SAT: Missing Renderer!\n";
        return;
    }
    if (!panel.texture) {
        debug_log << "SAT: Missing PANEL!\n";
        return;
    }

    char tempstr[30];
    // clear the box
    panel.Clear();
    SDL_FRect TextRect;
    TextRect.w=panel.dims.w/2;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    // render the satellite name

//    sprintf(tempstr, "%s", sat.get_name().c_str());
//    panel.render_text(TextRect, Sans, sat.color, tempstr);
    panel.render_text(TextRect, Sans, sat.color, sat.get_name().c_str());
    // if we have a pass coming, render its start and end times
    time_t pass_time = sat.pass_start();
    if (pass_time) {
        tm* test_time;
        TextRect.y=panel.dims.h - (panel.dims.h/11)-4;
        TextRect.w=panel.dims.w /3;
        test_time = localtime(&pass_time);
        strftime(tempstr, 12, "%H:%M", test_time);
        panel.render_text(TextRect, Sans, sat.color, tempstr);
        TextRect.x=panel.dims.w - (panel.dims.w/3);
        pass_time = sat.pass_end();
        test_time = localtime(&pass_time);
        strftime(tempstr, 12, "%H:%M", test_time);
        panel.render_text(TextRect, Sans, sat.color, tempstr);
    }

    // render the crosshairs and target pass chart
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    std::vector<SDL_FPoint> circle_pts;
    float radius = panel.dims.w/2;
    if (panel.dims.h < panel.dims.w) {
        radius = panel.dims.h/2;
    }
    radius *= 0.8f;
    std::vector<SDL_FPoint> pass_pts;
    SDL_FRect pass_box;
    pass_box.x=(panel.dims.w - (2*radius))/2;
    pass_box.y=(panel.dims.h - (2*radius))/2;
    pass_box.w=2*radius;
    pass_box.h=2*radius;

    // cross hairs
    SDL_FPoint center = SDL_FPoint{panel.dims.w/2, panel.dims.h/2};
    SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 64, 0, 255);
    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x-radius, center.y);
    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x+radius, center.y);
    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x, center.y+radius);
    SDL_RenderLine(panel.GetRenderer(), center.x, center.y, center.x, center.y-radius);

    // concentric degree circles
    SDL_SetRenderDrawColor(panel.GetRenderer(), 64, 0, 64, 255);
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius *=3;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.GetRenderer(), circle_pts.data(), static_cast<int>(circle_pts.size()));
    SDL_RenderPoint(panel.GetRenderer(), center.x, center.y);

    // draw the pass trajectory
    sat.draw_pass(sat.pass_start(), sat.pass_end(),  &pass_pts, &pass_box);
    SDL_SetRenderDrawColor(panel.GetRenderer(), 0, 128, 0, 255);
    SDL_RenderLines(panel.GetRenderer(), pass_pts.data(), static_cast<int>(pass_pts.size()));


    return;
}

struct tle_cache {
    char name[80];
    char line1[80];
    char line2[80];
    SDL_Color color;
    bool draw;
};

std::string sat_json_parser(const char* input_string) {
    if (!input_string || ! input_string[0]) {
        return "";
    }
    std::ostringstream cache_stream;
    std::istringstream iostring_buffer;
    std::string sanitized(input_string);  // Make a copy (if amateur_tle is char*)
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\r'), sanitized.end());
    iostring_buffer.clear();
    iostring_buffer.str(sanitized);
    SDL_Color trackcols = {255,0,0,255};
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
            debug_log << "SAT TRACKER: Processing: " << new_cache.name << " ";
            for (const std::string& stropt : clockconfig.Sats()) {
                instring = new_cache.name;
                if (instring.compare(0,stropt.length(),stropt)==0) {
        	    new_cache.draw=true;
                    new_cache.color = trackcols;
                }
            }
            if (new_cache.draw) {
                debug_log << "Setting to draw \n";
            } else {
                debug_log << "\n";
            }
            cache_stream.write(reinterpret_cast<const char*>(&new_cache), sizeof(new_cache));
        } else { break; }
    }
    return (cache_stream.str());
}

Uint16 pass_pager[2] = {0,0};
std::vector<TrackedSatellite> satlist;
/*
  Need to change this to run as a truely independant thread
*/
int SDLCALL fetch_celestrak(void* data) {
  (void) data;

  char* amateur_tle = 0 ;
  Uint64 data_size;
        SDL_Log ("Fetching Satellite telemetry from Celestrak");
        debug_log << "SAT TRACKER: Fetching Satellite telemetry from Celestrak\n";
        data_size = http_loader("https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=tle", (void**)&amateur_tle);   //
//        data_size = http_loader("http://maincoon.aaediwen/celestrak.txt",  (void**)&amateur_tle);
        SDL_LockMutex(mutexes[MUTEX_CELESTRAK]);
        satlist.clear();
        if (data_size) {
            debug_log << "SAT TRACKER: Fetched New Sat data\n";
            std::string blob = sat_json_parser(amateur_tle);
            debug_log << "SAT TRACKER: caching "<< blob.length() << " Bytes of Sat Data\n";
            add_data_cache(MOD_SAT, blob.length(), (void*)blob.data());
            satlist.clear();
            if (amateur_tle) {
                free(amateur_tle);
                amateur_tle=0;
            }
            fetch_result = 2;
        } // we got input data
        else { fetch_result = 3; }
        SDL_UnlockMutex(mutexes[MUTEX_CELESTRAK]);
        return 0;
}

Uint32 SDLCALL fetch_celestrak (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)userdata;
    (void)interval;
     if (timerID) {
         SDL_LockMutex(mutexes[MUTEX_CELESTRAK]);
         fetch_result = 10;
         SDL_UnlockMutex(mutexes[MUTEX_CELESTRAK]);
         SDL_Thread* thread = SDL_CreateThread(fetch_celestrak, "Celestrak Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "Failed to Create Sat Data Fetch Thread\n";
          }
//          interval = 3600000 ;
//          if (result) {
//              return (interval * 6);
//          } else {
//              return (interval * 2);
//          }
     }
     sat_timer = 0;
     return 0;
}


void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map) {
    ScreenFrame* overlay;
    std::istringstream tle_raw;
    char* amateur_tle = 0 ;
    Uint64 data_size;
    time_t cache_time;
    SDL_LockMutex(mutexes[MUTEX_CELESTRAK]);
    if (!sat_timer) {
        Uint32 interval;
        interval = 3600000 ;

        switch (fetch_result) {
            case 0:
                sat_timer = SDL_AddTimer(30000, fetch_celestrak, NULL);
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
    SDL_UnlockMutex(mutexes[MUTEX_CELESTRAK]);
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Sat Module during resize event!");
        return;
    }
    if (!font) {
        debug_log << "SAT: No font defined\n";
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "SAT: Missing Renderer!\n";
        return;
    }
    if (!panel.texture) {
        debug_log << "SAT: Missing PANEL!\n";
        return;
    }

    SDL_FRect TextRect;
    delete_owner_pins(MOD_SAT);
    bool reload_flag = false;
    data_size = cache_loader(MOD_SAT, (void**)&amateur_tle, &cache_time);

    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
        if (amateur_tle) {
            free (amateur_tle);
            amateur_tle=0;
        }
    }
    if (reload_flag) {	// fetch new
/*        std::ostringstream cache_stream;

        data_size=0;
        if (data_size) {
            debug_log << "SAT TRACKER: Fetched New Sat data\n";
            std::string blob = sat_json_parser(amateur_tle);
            debug_log << "SAT TRACKER: caching "<< blob.length() << " Bytes of Sat Data\n";
            add_data_cache(MOD_SAT, blob.length(), (void*)blob.data());
            data_size = blob.length();
            tle_raw.clear();
            tle_raw.str(blob);
            if (amateur_tle) {
                free(amateur_tle);
                amateur_tle=0;
            }
        } // we got input data */
    } else {	// use cache[D
        tle_raw.clear();
        std::string sanitized(amateur_tle, data_size);
        tle_raw.str(sanitized);
        if (amateur_tle) {
            free(amateur_tle);
            amateur_tle=0;
        }
        debug_log <<"SAT TRACKER: Using "<< data_size << " Bytes of Cached Data!\n";
    }

    // clear the box
    panel.Clear();
    SDL_FRect mapsize ;
    mapsize.w = map.dims.w;
    mapsize.h = map.dims.h;
    bool redraw_flag = false;
    redraw_flag = (!overlays.overlay_check(MOD_SAT));

    // render the header
    TextRect.w=panel.dims.w/2-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    panel.render_text(TextRect, font, {128,128,0,255}, "SAT TRACKERS");
    TextRect.w=panel.dims.w-10;
    TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));

    if (data_size) {
        debug_log << "SAT TRACKER: We have tracking data: "<< data_size << " Bytes";
    } else {
        debug_log <<"Tracking Data Fetch Error!\n";
        TextRect.w=panel.dims.w-10;
        TextRect.h=panel.dims.h/11;
        TextRect.x=5;
        TextRect.y=panel.dims.h/10;
        if (TextRect.w > 5) {
            panel.render_text(TextRect, font, {128,128,0,255}, "NO SAT DATA");
        }
        return;
    }

    libsgp4::Observer obs(clockconfig.DE().latitude, clockconfig.DE().longitude, 0.27); // need a way to manage altitude here (last arg)
    // read the TLE data from Celestrak
    debug_log << "SAT_TRACKER: Reading Sat lists from Celestrak\n";
    struct tle_cache temp;
    while (tle_raw.read(reinterpret_cast<char*>(&temp), sizeof(temp))) {
        temp.name[49]=0;
        temp.line1[69]=0;
        temp.line2[69]=0;
        // read the TLE for a sat
        // is it one we want to show?
        debug_log << "SAT TRACKER: Read Sat " << temp.name << " with draw=" << temp.draw << "\n";
        bool draw_flag=temp.draw;
        // check if the sat exists in satlist
        TrackedSatellite *nextsat = nullptr;
        if (draw_flag) {

            std::string name(temp.name);
            std::string line1(temp.line1);
            std::string line2(temp.line2);
            SDL_LockMutex(mutexes[MUTEX_CELESTRAK]);
            for (TrackedSatellite& sat : satlist) {
                if (name.compare(0,sat.get_name().length(),sat.get_name())==0) {
                    nextsat = &sat;
                }
            }

            if (nextsat) {
                debug_log << "SAT TRACKER: Found NextSat: " << temp.name << "\n";
                if (reload_flag) {
                    nextsat->new_tracking(name, line1, line2);
                    nextsat->gen_telemetry(30, obs);
                    redraw_flag = true;
                }
            } else {
                debug_log << "SAT TRACKER:Creating New Sat entry with:\nSAT_TRACKER: "
                          << temp.name << "\nSAT_TRACKER: "
                          << temp.line1 << "\nSAT TRACKER: "
                          << temp.line2 << "\n";
                try {
                    nextsat = new TrackedSatellite(temp.name, temp.line1, temp.line2);
                    nextsat->color=temp.color;
                    debug_log << "SAT TRACKER: Regenerate track for " << temp.name << "\n";
                    if (nextsat->gen_telemetry(30, obs)) {
                        satlist.push_back(std::move(*nextsat));
                        delete (nextsat);
                        nextsat = nullptr;
                        redraw_flag= true;
                    }
                } catch (const std::exception& e){
                    SDL_Log ("Failed to create Satellite tracking entry for %s\n%s", temp.name, e.what());
                    debug_log << "SAT_TRACKER: Failed to create Satellite tracking entry for " << temp.name
                              << "\nSAT TRACKER: " << e.what() << "\n";
                }
            }
            SDL_UnlockMutex(mutexes[MUTEX_CELESTRAK]);
        }
//        SDL_Log ("Done with Sar %s", temp.name);
    } // read from Celestrak
    if (amateur_tle) {
        free(amateur_tle);
        amateur_tle = nullptr;
    }
    debug_log << "SAT_TRACKER: Displaying Selected Satellites\n";
    // display the selected satellites
    SDL_LockMutex(mutexes[MUTEX_CELESTRAK]);
    if (!satlist.empty()){
        if (pass_pager[0] >= satlist.size()) {
            pass_pager[0]=0;
        }
        pass_tracker(panel, satlist[pass_pager[0]]);
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
        overlay = overlays.get_overlay(panel.GetRenderer(), MOD_SAT, mapsize);
        if (redraw_flag) {
            overlay->Clear(SDL_Color{0,0,0,0});
        }
        for (TrackedSatellite& Sat : satlist) {
            if (redraw_flag) {
                debug_log << "SAT_TRACKER: Redrawing track for " << Sat.get_name().c_str() << "\n";
                Sat.draw_telemetry(*overlay);
            }
            // plot the sat's current location
            struct map_pin sat_pin;
            SDL_FPoint sat_loc;
            Sat.location(&sat_loc);
            sat_pin.owner   =               MOD_SAT;
            sprintf(sat_pin.label, "%s", Sat.get_name().c_str());
            sat_pin.lat     =               sat_loc.x;
            sat_pin.lon     =               sat_loc.y;
            sat_pin.icon    =               icon_bin.get_icon(map_icons::ICON_SAT);
            debug_log << "SAT TRACKER: got pin " <<  sat_pin.icon << "\n";
            sat_pin.color   =               Sat.color;;
            sat_pin.tooltip[0]      =               0;
            add_pin(&sat_pin);
        }

        debug_log << "SAT TRACKER: Loaded "<< satlist.size() << " SATS\n";
    } else {
        panel.render_text(SDL_FRect {panel.dims.w/20, panel.dims.h/4, (panel.dims.w/10)*8, panel.dims.h/10}, font, {128,128,0,255}, "NO SELECTED SATS");
    }
    SDL_UnlockMutex(mutexes[MUTEX_CELESTRAK]);
    return;
}

