#ifndef MODULES_H

#define MODULES_H
#include "utils.h"

#include <libsgp4/SGP4.h>
#include <libsgp4/Observer.h>
#include <libsgp4/CoordTopocentric.h>

class TrackedSatellite {

    private:
        struct SatTelemetry {
            double lat;
            double lon;
            double azimuth;     // compass direction from DE
            double elevation;   // elevation above horizon relative to DE
            time_t timestamp;
        };

        std::string name;
        std::string tle1;
        std::string tle2;
        libsgp4::Tle sat_tle;
        libsgp4::SGP4 sgp4;
        std::vector<struct SatTelemetry> telemetry;

    public:
        SDL_Color color;

        TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2): name(source_name), tle1(l1), tle2(l2), sat_tle(name, tle1, tle2), sgp4(sat_tle) {

//              SDL_Log("IN CONSTRUCTOR:\n%s: \t %i\n%s: \t %i\n%s: \t %i", source_name.c_str(), source_name.length(), l1.c_str(), l1.length(), l2.c_str(), l2.length());
        };

        ~TrackedSatellite() {};

        std::string get_name() {
            return (this->name);
        }

        time_t pass_start() {
            for (SatTelemetry point : this->telemetry) {
                if (point.elevation >0) {
                    return point.timestamp;
                }
            }
            return 0;
        }

        time_t pass_end() {
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

        void draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<SDL_FPoint> *pass_pts, const SDL_FRect *size) {
            if (!pass_pts) {
                return;
            }
            pass_pts->clear();

            bool pass_entry=false;
            int pass_samples=0;
            for (SatTelemetry point : this->telemetry) {
                if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
                    pass_samples++;
                }
            }
            SDL_Log("Rendering %i samples for pass path of %s", pass_samples, this->name.c_str());
            float max_radius = size->w/2;
            if (size->h < size->w) {
                max_radius = size->h/2;
            }
            SDL_FPoint center, new_point;
            center.x = (size->w/2)+size->x;
            center.y = (size->h/2)+size->y;
            for (SatTelemetry point : this->telemetry) {
                 if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
                     float radius = max_radius * (1-point.elevation/90);
                     new_point.x = center.x + radius * sinf(point.azimuth*(M_PI/180.0));
                     new_point.y = center.y - radius * cosf(point.azimuth*(M_PI/180.0));
//                     SDL_Log("AZ: %.2f°, EL: %.2f° → Radius %.1f", point.azimuth, point.elevation, radius);
                     pass_pts->push_back(new_point);
                }
            }
            return;
        }

        void add_telemetry(const double lat,const double lon, const double elevation, const double azimuth, const time_t timestamp) {
            struct SatTelemetry new_node;
            new_node.lat=lat;
            new_node.lon=lon;
            new_node.elevation=elevation;
            new_node.azimuth=azimuth;
            new_node.timestamp=timestamp;
            telemetry.push_back(new_node);
            return;
        }
        void location (SDL_FPoint *result) {
            libsgp4::DateTime dt = libsgp4::DateTime::Now();
            libsgp4::Eci eci = this->sgp4.FindPosition(dt);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();
            result->x = geo.latitude * (180/M_PI);
            result->y = geo.longitude * (180/M_PI);
            return ;
        }


        bool gen_telemetry(const int resolution, libsgp4::Observer& obs) {
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


        void draw_telemetry(ScreenFrame& map) {

            if (this->telemetry.empty()) { return; }

            SDL_SetRenderTarget(surface, map.texture);
            SDL_SetRenderDrawColor(surface, this->color.r, this->color.g, this->color.b, this->color.a);
            SDL_FPoint* SDLPoints = (SDL_FPoint*)malloc(sizeof(SDL_FPoint)*this->telemetry.size());

            int index=0;
            int render_size=0;
            int xt, yt;
            xt = static_cast<int>(map.dims.w);
            yt = static_cast<int>(map.dims.h);

            for (SatTelemetry point : this->telemetry) {
                SDL_FPoint map_cords;
                SDL_FPoint *last_point = nullptr;

                // calculate the current point
                cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
                render_size++;
                if (point.elevation >0) {
                    SDL_SetRenderDrawColor(surface, 0, 0, 128, 255);
                    SDL_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 4.0, 4.0};
                    SDL_RenderFillRect(surface, &visirect);
                    SDL_SetRenderDrawColor(surface, this->color.r, this->color.g, this->color.b, this->color.a);
                }
                if (index >1) { // if we have a last point to compare to
                    if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/4)) {      // and the delta is greater than 100
                        //render the current segment
                        SDL_RenderLines(surface, SDLPoints, render_size-1);
                        //reset the index
                        index=0;
                        render_size=1;
                        // re-gen the current pixel
                        cords_to_px(point.lat, point.lon, xt, yt, &(SDLPoints[index]));
//                        SDL_Log("Spawning a new spline");
                    }
                }
                index++;
            }

            SDL_RenderLines(surface, SDLPoints, render_size);
            free (SDLPoints);
            SDL_SetRenderTarget(surface, NULL);
            SDL_RenderTexture(surface, map.texture, NULL, &(map.dims));
            return;
        }
};





void draw_de_dx(ScreenFrame& panel, TTF_Font* font, double lat, double lon, int de_dx);
void draw_callsign(ScreenFrame& panel, TTF_Font* font, const char* callsign);
void load_maps();
int draw_map(ScreenFrame& panel);
int draw_clock(ScreenFrame& panel, TTF_Font* font);
void pota_spots(ScreenFrame& panel, TTF_Font* font);
void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map);


#endif

