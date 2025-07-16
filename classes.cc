#include "aaediclock.h"
#include "utils.h"
#include "modules.h"
#include <sstream>
#include "json.hpp"
#include <libsgp4/CoordTopocentric.h>

using json = nlohmann::json;


ScreenFrame::ScreenFrame() {
    this->dims.x=0;
    this->dims.y=0;
    this->dims.h=0;
    this->dims.w=0;


    this->texture = 0;
    this->surface = 0;
    this->renderer = 0;
}

ScreenFrame::~ScreenFrame() {
    Reset();
}

bool ScreenFrame::Create (SDL_Renderer* parent, SDL_FRect size) {
    dims=size;
    int h = static_cast<int>(size.h);
    int w = static_cast<int>(size.w);
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTexture (parent, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                       w, h );
    if (!this->texture) {
        SDL_Log("Error Creating Texture!");
        return false;
    }
    renderer=parent;
    Clear();

    return true;
}

void ScreenFrame::Reset() {
    if (this->texture) {
        SDL_DestroyTexture(this->texture);
    }
    if (this->surface) {
        SDL_DestroySurface(this->surface);
    }
    this->texture=nullptr;
    this->surface=nullptr;
    this->renderer = nullptr;
    this->dims = {};
    return;
}

void ScreenFrame::draw_border() {
    if (texture && renderer) {
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
        SDL_FRect border;
        border.x=0;
        border.y=0;
        border.w=dims.w;
        border.h=dims.h;
        SDL_SetRenderTarget(renderer, texture);
        SDL_RenderRect(renderer, &(border));
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderTexture(renderer, texture, NULL, &(dims));
    }
    else {
        SDL_Log("Bad renderer or texture on border draw");
    }
    return;
}

void ScreenFrame::render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const char* str) {
    if (texture && renderer) {
    SDL_Surface* textsurface;
    SDL_Texture *TextTexture;

    // render a text string
    textsurface = TTF_RenderText_Shaded(font, str, strlen(str), color, SDL_Color{0,0,0,0});
//    textsurface = TTF_RenderText_LCD(font, str, strlen(str), color, bg_color);
    if (textsurface==NULL) {
        SDL_Log("Text render error: %s", SDL_GetError());
        return;
    }
    TextTexture = SDL_CreateTextureFromSurface(renderer, textsurface);
    SDL_SetRenderTarget(renderer, texture);
    SDL_RenderTexture(renderer, TextTexture, NULL, &text_box);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

}
    else {
        SDL_Log("Bad renderer or texture on Text Render");
        }

}


void ScreenFrame::Clear(const SDL_Color& color) {
    // clear the box
    if (renderer && texture) {
        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);  // Clear solid
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderClear(renderer);  // Fills the entire target with the draw color
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);  // Clear solid
        SDL_SetRenderTarget(renderer, NULL);
    }
    else {
        SDL_Log("Bad Renderer or Texture on Clear");
    }
    return;
}

void config::qrz_sesskey() {
    char* xml = 0 ;
    Uint32 key_size;
    //std::string url = "https://xmldata.qrz.com/xml/current/?username="+m_CallSign+"&password="+m_QRZ.Secret;
    std::string url = "https://xmldata.qrz.com/xml/current/?username=" + m_CallSign + ";password=" + m_QRZ.Secret;
    key_size = http_loader(url.c_str(), &xml);
    if (key_size) {
        // parse XML for session key
        std::istringstream stream(xml);
        std::string keyline;
        size_t tag_start, tag_stop;
        while (std::getline(stream, keyline)) {
            tag_start=keyline.find("<Key>");
            tag_stop=keyline.find("</Key>");
            if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
                tag_start +=5;
                m_QRZ.Key = keyline.substr(tag_start, tag_stop - tag_start);
                printf ("Loaded QRZ Session Key\n");
            }

            tag_start=keyline.find("<Error>");
            tag_stop=keyline.find("</Error>");
            if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
                tag_start +=7;
                std::string QRZ_Err = keyline.substr(tag_start, tag_stop - tag_start);
                printf ("QRZ Session Key Error: %s\n", QRZ_Err.c_str());
            }
        }
    }
    if (m_QRZ.Key.empty()) {
        printf ("Failed to load QRZ Session Key!\n");
    }
    return;
}

void config::write_config() {
    json data = json({});
    data["CallSign"]=m_CallSign.c_str();
    data["DE"]["Latitude"]=m_DE.latitude;
    data["DE"]["Longitude"]=m_DE.longitude;
    data["DX"]["Latitude"]=m_DX.latitude;
    data["DX"]["Longitude"]=m_DX.longitude;
    std::vector<std::uint8_t> QRZ_secret;
    QRZ_secret = json::to_cbor(m_QRZ.Secret);
    for (size_t i = 0; i < QRZ_secret.size(); ++i) {
        QRZ_secret[i] ^= static_cast<uint8_t>(i);
    }
    data["QRZ"]=QRZ_secret;
    data["SatList"]=m_sats;
    std::ofstream f("aaediclock_config.json");
    f << data.dump(5);
    f.close();

    return;


}

void config::load_config() {
    bool goodread = false;
    json data;

    m_CallSign = "N0CALL";
    m_sats.clear();
    m_DE={0, 0};
    m_DX={0, 0};
    m_QRZ.Secret.clear();
    m_QRZ.Key.clear();

    SDL_Log ("Loading CONFIG");
    std::ifstream f("aaediclock_config.json");
    if (f.good()) {
        try {
            f >> data;          // parse the json
            goodread=true;
        } catch (const json::parse_error &e) {
            printf ("JSON parse error: %s\n",  e.what());
            goodread=false;
        }
    } else {
        printf ("Config File Read error\n");
        goodread=false;
    }

    if (goodread) {
        printf ("Reading CONFIG\n");
        if (data.contains("DE")) {
            if (data["DE"].contains("Latitude") && data["DE"].contains("Longitude")) {
                if (data["DE"]["Latitude"].is_number() && data["DE"]["Longitude"].is_number() ) {
                    m_DE.latitude=data["DE"]["Latitude"];
                    m_DE.longitude=data["DE"]["Longitude"];
                }
            }
        }
        if (data.contains("DX")) {
            if (data["DX"].contains("Latitude") && data["DX"].contains("Longitude")) {
                if (data["DX"]["Latitude"].is_number() && data["DX"]["Longitude"].is_number() ) {
                    m_DX.latitude=data["DX"]["Latitude"];
                    m_DX.longitude=data["DX"]["Longitude"];
                }
            }
        }

        if (data.contains("CallSign")) {
            if (data["CallSign"].is_string()) {
                m_CallSign=data["CallSign"];
            }
        }
        if (data.contains("QRZ")) {
            if (data["QRZ"].is_string()) {
                m_QRZ.Secret=data["QRZ"];
                qrz_sesskey();
            } else if (data["QRZ"].is_array()) {
                std::vector<std::uint8_t> QRZ_secret;
                QRZ_secret = data["QRZ"].get<std::vector<std::uint8_t>>();
                for (size_t i = 0; i < QRZ_secret.size(); ++i) {
                    QRZ_secret[i] ^= static_cast<uint8_t>(i);
                }
                m_QRZ.Secret = json::from_cbor(QRZ_secret).get<std::string>();
                qrz_sesskey();
            }
        }

        if (data.contains("SatList")) {
            if (data["SatList"].is_array()) {
                for (const auto& item : data["SatList"]) {
                    if (item.is_string()) {
                        m_sats.push_back(item);
                    }
                }
            }
        }
    } else {
        printf ("ERROR Reading CONFIG. Defaults used\n");
    }
    return;
} // loadconfig

config::config() {
    load_config();
}

config::~config() {}

void config::set_qrz_pass(const std::string& newpass) {
    m_QRZ.Secret=newpass;
    write_config();
}

const std::string& config::CallSign() const {
    return m_CallSign;
}

const GeoCoord& config::DE() const {
    return m_DE;
}

const GeoCoord& config::DX() const {
    return m_DX;
}

const std::vector<std::string>& config::Sats() const {
    return m_sats;
}

const std::string& config::qrz_key(bool refresh) {
    if (refresh) {
        qrz_sesskey();
    }
    return m_QRZ.Key;
}


dxspot::dxspot() {
    qrz_valid = false;
    country.clear();
};

dxspot::~dxspot() {};

void dxspot::find_mode () {
    mode.clear();
    if (note.empty()) {
        return;
    }
    std::string mode_parent = note;
    std::transform(mode_parent.begin(), mode_parent.end(), mode_parent.begin(), ::toupper);
    if (mode_parent.find("FT8") != std::string::npos) mode =  "FT8";
    if (mode_parent.find("FT4") != std::string::npos) mode =  "FT4";
    if (mode_parent.find("CW") != std::string::npos) mode = "CW";
    if (mode_parent.find("SSB") != std::string::npos) mode =  "SSB";
    if (mode_parent.find("USB") != std::string::npos) mode =  "USB";
    if (mode_parent.find("LSB") != std::string::npos) mode =  "LSB";
    if (mode_parent.find("RTTY") != std::string::npos) mode =  "RTTY";
    return;
}

void dxspot::fill_qrz() {
    query_qrz();
}

void dxspot::display_spot(ScreenFrame& panel, int y, int max_age) {
    // add to screen list
//     SDL_Log("adding list");
       char tempstr[128];
       SDL_Color tempcolor={128,0,0,0};
       SDL_FRect TextRect;
       TextRect.x=2;
       TextRect.y= y * 1.0f;
       TextRect.w=panel.dims.w/4;
       TextRect.h=panel.dims.h/15;

       SDL_FRect age_rect;
       age_rect.h = TextRect.h/8;
       age_rect.y = y+((TextRect.h/8)*7);
       age_rect.x = 2;
       age_rect.w = (panel.dims.w-4)*(static_cast<float>(time(NULL)-timestamp)/max_age);
//       SDL_Log("Spot age: %li Seconds, Bar width: %f pixels", (time(NULL)-timestamp), age_rect.w );
       SDL_SetRenderTarget(panel.renderer, panel.texture);
       SDL_SetRenderDrawColor(surface, 128, 128, 0, 255);
       SDL_RenderFillRect(surface, &age_rect );
       SDL_SetRenderTarget(panel.renderer, NULL);
       panel.render_text(TextRect, Sans, tempcolor, dx.c_str());
       TextRect.x += (panel.dims.w/4)+2;
       sprintf(tempstr, "%4.3f", (frequency/1000));
       panel.render_text(TextRect, Sans, tempcolor, tempstr);
       TextRect.x += (panel.dims.w/4)+2;
       TextRect.w /=2;
       if (mode.size() >0) {
            panel.render_text(TextRect, Sans, tempcolor, mode.c_str());
       }
       TextRect.w += (panel.dims.w/4)-(panel.dims.w/20);
       TextRect.x += (panel.dims.w/8)+1;
       if (country.size() >0) {
       panel.render_text(TextRect, Sans, tempcolor, country.c_str());
       }
       TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
}

void dxspot::print_spot() {
    char timestr[128];
    struct tm *clocktime;
    clocktime = gmtime(&timestamp);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M", clocktime);
    SDL_Log ("DX: %s\t%lf\t Spotter: %s", dx.c_str(), frequency, spotter.c_str());
    SDL_Log ("Loc: %lf\t%lf\tMode %s", lat, lon, mode.c_str());
    SDL_Log ("Time: %s\nNote: %s", timestr, note.c_str());
};


void dxspot::query_qrz () {
    // query QRZ for a call location
    char* xml = 0 ;
    lat = 0.0;
    lon = 0.0;
    country.clear();
    bool lat_valid, lon_valid;
    lat_valid=false;
    lon_valid=false;
    Uint32 xml_size;
    if (dx.empty()) {
        return;
    }
    if (!clockconfig.qrz_key().empty()) {
        std::string url = "https://xmldata.qrz.com/xml?s=" + clockconfig.qrz_key() + ";callsign=" + dx;
        xml_size = http_loader(url.c_str(), &xml);
        if (xml_size) {
            // parse XML for session key
            std::istringstream stream(xml);
            std::string keyline;
            size_t tag_start, tag_stop;

            while (std::getline(stream, keyline)) {
                tag_start = keyline.find("<lat>");
                tag_stop = keyline.find("</lat>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 5;
                    lat = std::stod(keyline.substr(tag_start, tag_stop - tag_start));
                    lat_valid = true;
                }
                tag_start = keyline.find("<lon>");
                tag_stop = keyline.find("</lon>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 5;
                    lon = std::stod(keyline.substr(tag_start, tag_stop - tag_start));
                    lon_valid = true;
                }
                tag_start = keyline.find("<country>");
                tag_stop = keyline.find("</country>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 9;
                    country = keyline.substr(tag_start, tag_stop - tag_start);
                }

                tag_start = keyline.find("<Error>");
                tag_stop = keyline.find("</Error>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 7;
                    std::string QRZ_Err = keyline.substr(tag_start, tag_stop - tag_start);
                    printf("QRZ Call Lookup Error: %s\n", QRZ_Err.c_str());
                }
            }
        }
    }
    if (lat_valid && lon_valid) {
      qrz_valid=true;
    }
//    SDL_Log("DX  Coords from QRZ: %s, %f, %f", dx.c_str(), lat, lon);
    return;
}


TrackedSatellite::TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2): name(source_name), tle1(l1), tle2(l2), sat_tle(name, tle1, tle2), sgp4(sat_tle) {

};

TrackedSatellite::~TrackedSatellite() {};

std::string& TrackedSatellite::get_name() {
    return (this->name);
}

time_t TrackedSatellite::pass_start() {
    // get the start time for a satellite pass
    for (SatTelemetry point : this->telemetry) {
        if (point.elevation >0) {
            return point.timestamp;
        }
    }
    return 0;
}

time_t TrackedSatellite::pass_end() {
    // get the end time for a satellite pass
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
    if (!pass_pts) {
        return;
    }
    pass_pts->clear();

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


void TrackedSatellite::draw_telemetry(ScreenFrame& map) {
    // draw the satellite's telemetry track on the map
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
