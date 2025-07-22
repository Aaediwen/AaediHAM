#include <libsgp4/CoordTopocentric.h>

#include "sat_tracker.h"
#include "../aaediclock.h"
#include "../utils.h"

TrackedSatellite::TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2): name(source_name), tle1(l1), tle2(l2), sat_tle(name, tle1, tle2), sgp4(sat_tle) {

};

TrackedSatellite::~TrackedSatellite() {};

void TrackedSatellite::new_tracking(const std::string& source_name, const std::string& l1, const std::string& l2) {
    name = source_name;
    tle1=l1;
    tle2=l2;
    sat_tle = libsgp4::Tle(name, tle1, tle2);
    sgp4 = libsgp4::SGP4(sat_tle);
    telemetry.clear();

}



std::string& TrackedSatellite::get_name() {
    return (this->name);
}

time_t TrackedSatellite::pass_start() {
    // get the start time for a satellite pass
    if (telemetry.empty()) {
        return 0;
    }
    for (SatTelemetry point : this->telemetry) {
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
    for (SatTelemetry point : this->telemetry) {
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

    pass_pts->clear();
    if (!pass_pts) {
        return;
    }
    if (telemetry.empty()) {
        return;
    }
    int pass_samples=0;
    for (SatTelemetry point : this->telemetry) {
        if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
            pass_samples++;
        }
    }
//  SDL_Log("Rendering %i samples for pass path of %s", pass_samples, this->name.c_str());
    float max_radius = size->w/2;
    if (size->h < size->w) {
        max_radius = size->h/2;
    }
    SDL_FPoint center, new_point;
    center.x = (size->w/2)+size->x;
    center.y = (size->h/2)+size->y;
    for (SatTelemetry point : this->telemetry) {
        if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
            float radius = max_radius * (1-point.elevation/90.0f);
            new_point.x = center.x + radius * sinf(point.azimuth*(M_PI/180.0));
            new_point.y = center.y - radius * cosf(point.azimuth*(M_PI/180.0));
//          SDL_Log("AZ: %.2f°, EL: %.2f° → Radius %.1f", point.azimuth, point.elevation, radius);
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
    libsgp4::DateTime dt = libsgp4::DateTime::Now();
    libsgp4::Eci eci = this->sgp4.FindPosition(dt);
    libsgp4::CoordGeodetic geo = eci.ToGeodetic();
    result->x = geo.latitude * (180/M_PI);
    result->y = geo.longitude * (180/M_PI);
    return ;
}


bool TrackedSatellite::gen_telemetry(const int resolution, libsgp4::Observer& obs) {
    // generate the telemetry track for a satellite
    telemetry.clear();
    bool add_flag;
    add_flag=true;
    int i;
    double mean_motion =  this->sat_tle.MeanMotion();
    double period = (1440*60)/mean_motion; // period in seconds
    i = floor(period/resolution);
    for (int offset = 0 ; offset < (i*resolution) ; offset +=resolution) {
        try {
           libsgp4::DateTime dt = libsgp4::DateTime::Now().AddSeconds(offset);
        //    libsgp4::DateTime dt = this->sat_tle.Epoch().AddSeconds(offset);
            libsgp4::Eci eci = this->sgp4.FindPosition(dt);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();
            libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);
            double lat_deg = geo.latitude * (180/M_PI);
            double long_deg = geo.longitude * (180/M_PI);
            double elevation_deg = topo.elevation * (180/M_PI);
            double azimuth_deg = topo.azimuth * (180/M_PI);
            this->add_telemetry(lat_deg, long_deg, elevation_deg, azimuth_deg, (time(NULL)+offset));
        } catch (const libsgp4::SatelliteException& e) {
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
    // draw the satellite's telemetry track on the map
    if (this->telemetry.empty()) { return; }

    SDL_SetRenderTarget(map.renderer, map.texture);
    SDL_SetRenderDrawColor(map.renderer, this->color.r, this->color.g, this->color.b, this->color.a);
    SDL_FPoint* SDLPoints = (SDL_FPoint*)malloc(sizeof(SDL_FPoint)*this->telemetry.size());

    int index=0;
    int render_size=0;
    int xt, yt;
    xt = static_cast<int>(map.dims.w);
    yt = static_cast<int>(map.dims.h);
    for (SatTelemetry point : this->telemetry) {
        // calculate the current point
        cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
        render_size++;
        if (point.elevation >0) {
            SDL_SetRenderDrawColor(map.renderer, 0, 0, 128, 255);
            SDL_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 4.0, 4.0};
            SDL_RenderFillRect(map.renderer, &visirect);
            SDL_SetRenderDrawColor(map.renderer, this->color.r, this->color.g, this->color.b, this->color.a);
        }
        if (index >1) { // if we have a last point to compare to
            if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/4)) {      // and the delta is greater than 100
                //render the current segment
                SDL_RenderLines(map.renderer, SDLPoints, render_size-1);
                //reset the index
                index=0;
                render_size=1;
                // re-gen the current pixel
                cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
            }
        }
        index++;
    }

    SDL_RenderLines(map.renderer, SDLPoints, render_size);
    free (SDLPoints);
    map.present();
//    SDL_SetRenderTarget(map.renderer, NULL);
//    SDL_RenderTexture(map.renderer, map.texture, NULL, &(map.dims));
    return;
}





void circle_helper(std::vector<SDL_FPoint> *circle_points, int radius, SDL_FPoint center, int segments) {
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * M_PI * i) / segments;
        SDL_FPoint pt = {
            center.x + radius * cosf(theta),
            center.y + radius * sinf(theta)
        };
        circle_points->push_back(pt);
    }
    return;
}

void pass_tracker(ScreenFrame& panel, TrackedSatellite& sat) {
    // clear the box
    panel.Clear();

    char tempstr[30];
    SDL_FRect TextRect;
    TextRect.w=panel.dims.w/2;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    sprintf(tempstr, "%s", sat.get_name().c_str());
    panel.render_text(TextRect, Sans, sat.color, tempstr);
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

    SDL_SetRenderTarget(panel.renderer, panel.texture);
    std::vector<SDL_FPoint> circle_pts;
    float radius = panel.dims.w/2;
    if (panel.dims.h < panel.dims.w) {
        radius = panel.dims.h/2;
    }
    radius *=.8;
    std::vector<SDL_FPoint> pass_pts;
    SDL_FRect pass_box;
    pass_box.x=(panel.dims.w - (2*radius))/2;
    pass_box.y=(panel.dims.h - (2*radius))/2;
    pass_box.w=2*radius;
    pass_box.h=2*radius;


    SDL_FPoint center = SDL_FPoint{panel.dims.w/2, panel.dims.h/2};
    SDL_SetRenderDrawColor(panel.renderer, 64, 64, 0, 255);
    SDL_RenderLine(panel.renderer, center.x, center.y, center.x-radius, center.y);
    SDL_RenderLine(panel.renderer, center.x, center.y, center.x+radius, center.y);
    SDL_RenderLine(panel.renderer, center.x, center.y, center.x, center.y+radius);
    SDL_RenderLine(panel.renderer, center.x, center.y, center.x, center.y-radius);
    SDL_SetRenderDrawColor(panel.renderer, 64, 0, 64, 255);
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.renderer, circle_pts.data(), circle_pts.size());
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.renderer, circle_pts.data(), circle_pts.size());
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.renderer, circle_pts.data(), circle_pts.size());
    circle_pts.clear();
    radius *=3;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(panel.renderer, circle_pts.data(), circle_pts.size());
    SDL_RenderPoint(panel.renderer, center.x, center.y);
    sat.draw_pass(sat.pass_start(), sat.pass_end(),  &pass_pts, &pass_box);
    SDL_SetRenderDrawColor(panel.renderer, 0, 128, 0, 255);
    SDL_RenderLines(panel.renderer, pass_pts.data(), pass_pts.size());


    return;
}


Uint16 pass_pager[2] = {0,0};
std::vector<TrackedSatellite> satlist;
void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map) {
    char* amateur_tle = 0 ;
    Uint32 data_size;
    time_t cache_time;

    SDL_FRect TextRect;
//    SDL_Log ("In Sat Trracker Module");
    delete_owner_pins(MOD_SAT);
    bool reload_flag = false;
    data_size = cache_loader(MOD_SAT, &amateur_tle, &cache_time);
    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
    }

    if (reload_flag) {
//        data_size = http_loader("https://aaediwen.theaudioauthority.net/morse/celestrak", &amateur_tle);      // debug
        data_size = http_loader("https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=tle", &amateur_tle);   // live
        satlist.clear();
        if (data_size) {
            add_data_cache(MOD_SAT, data_size, amateur_tle);
        }
    }

//    SDL_Log ("Read Sat List");
    // clear the box
    panel.Clear();
//    SDL_SetRenderTarget(panel.renderer, panel.texture);

    // render the header
    TextRect.w=panel.dims.w/2-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    panel.render_text(TextRect, font, {128,128,0,255}, "SAT TRACKERS");
    TextRect.w=panel.dims.w-10;
    TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));

    if (data_size) {
//      SDL_Log ("We have tracking data: %i Bytes", data_size);
    } else {
        SDL_Log ("Tracking Data Fetch Error!");
        return;
    }


    Uint32 read_flag = 1;
    std::istringstream iostring_buffer;
    std::string sanitized(amateur_tle);  // Make a copy (if amateur_tle is char*)
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\r'), sanitized.end());
    iostring_buffer.clear();
    iostring_buffer.str(sanitized);
    std::string temp[3]; // name line1, line 2

    libsgp4::Observer obs(clockconfig.DE().latitude, clockconfig.DE().longitude, 0.27); // need a way to manage altitude here (last arg)
    SDL_Color trackcols = {255,0,0,255};
    // read the TLE data from Celestrak
//    SDL_Log ("Reading Sat lists from Celestrak");
    while (read_flag) {
        // read the TLE for a sat
        std::getline(iostring_buffer, temp[0]);
        std::getline(iostring_buffer, temp[1]);
        std::getline(iostring_buffer, temp[2]);
        if (temp[0].length() && temp[1].length() && temp[2].length()) {
            // is it one we want to show?
            bool draw_flag=false;
            for (const std::string& stropt : clockconfig.Sats()) {
                if (temp[0].compare(0,stropt.length(),stropt)==0) {
                    draw_flag=true;
                }
            }
            // check if the sat exists in satlist
             TrackedSatellite *nextsat = nullptr;
             if (draw_flag) {
                 for (TrackedSatellite& sat : satlist) {
                    if (temp[0].compare(0,sat.get_name().length(),sat.get_name())==0) {
                        nextsat = &sat;
                    }
                }
            }
            if (nextsat) {
                if (reload_flag) {
                    nextsat->new_tracking(temp[0], temp[1], temp[2]);
                    nextsat->gen_telemetry(30, obs);
                }
            } else {

                trackcols.r -= 20;
                trackcols.g += 20;
                trackcols.b += 10;
                // if so, calculate the track for it
                if (draw_flag) {
                    nextsat = new TrackedSatellite(temp[0], temp[1], temp[2]);
//                    TrackedSatellite nextsat(temp[0], temp[1], temp[2]);
                    nextsat->color=trackcols;
//                    SDL_Log ("Regenerate track for %s", temp[0].c_str());
                    if (nextsat->gen_telemetry(30, obs)) {
                        satlist.push_back(*nextsat);
                    }
                }
            }
        } else { break; }
    } // read from Celestrak
    if (amateur_tle) {
        free(amateur_tle);
        amateur_tle = nullptr;
    }
//    SDL_Log("Displaying Selected Satellites");
    // display the selected satellites
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
            bool draw_flag=false;
            for (const std::string& stropt : clockconfig.Sats()) {
                if (Sat.get_name().compare(0,stropt.length(),stropt)==0) {
                    draw_flag=true;
                }
            }
            if (draw_flag) {
                const time_t time_now = time(NULL);
                if ((Sat.telemetry_age() - time_now) < 60) {
                    Sat.gen_telemetry(30, obs);
                }
                Sat.draw_telemetry(map);
                // plot the sat's current location
                struct map_pin sat_pin;
                SDL_FPoint sat_loc;
                Sat.location(&sat_loc);
                sat_pin.owner   =               MOD_SAT;
                sprintf(sat_pin.label, "%s", Sat.get_name().c_str());
                sat_pin.lat     =               sat_loc.x;
                sat_pin.lon     =               sat_loc.y;
                sat_pin.icon    =               0;
                sat_pin.color   =               Sat.color;;
                sat_pin.tooltip[0]      =               0;
                add_pin(&sat_pin);
            }
        }

//        SDL_Log("Loaded %li SATS", satlist.size());
    }

    return;
}

