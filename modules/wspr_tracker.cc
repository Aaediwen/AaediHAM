
#include "wspr_tracker.h"
#include "../aaediclock.h"
#include "../utils.h"
#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#endif
#include <cstring>
#include <sstream>



TrackedWSPR::TrackedWSPR(const std::string& tx_call, TrackedWSPR::Band band, time_t start = 0) {
    m_tx_sign = tx_call;
    m_band = band;
    m_start_time = start;
    m_telemetry.clear();
}

TrackedWSPR::~TrackedWSPR() {
    m_telemetry.clear();
};

TrackedWSPR::TrackedWSPR(TrackedWSPR&& source) noexcept {	// move constructor -- C++11
    m_tx_sign = std::move(source.m_tx_sign);
    m_band = std::move(source.m_band);
    m_start_time = std::move(source.m_start_time);
    m_color = std::move(source.m_color);
    m_telemetry = std::move(source.m_telemetry);

}


TrackedWSPR& TrackedWSPR::operator=(TrackedWSPR&& source) noexcept {    // move with overwrite -- C++11
    if (this != &source) {
        m_tx_sign.clear();
        m_tx_sign = std::move(source.m_tx_sign);
        m_start_time = 0;
        m_start_time = std::move(source.m_start_time);
        m_band = std::move(source.m_band);
        m_color = {0, 0, 0, 0};
        m_color = std::move(source.m_color);
        m_telemetry.clear();
        m_telemetry = std::move(source.m_telemetry);
    }
    return (*this);
}

TrackedWSPR::TrackedWSPR(const TrackedWSPR& source) {              // copy to new
    m_tx_sign = source.m_tx_sign;
    m_band = source.m_band;
    m_start_time = source.m_start_time;
    m_color = source.m_color;
    m_telemetry = source.m_telemetry;

}

TrackedWSPR& TrackedWSPR::operator=(const TrackedWSPR& source) { // copy with overwrite
    if (this != &source) {
        m_tx_sign.clear();
        m_tx_sign = source.m_tx_sign;
        m_start_time = 0;
        m_start_time = source.m_start_time;
        m_band = source.m_band;
        m_color = {0, 0, 0, 0};
        m_color = source.m_color;
        m_telemetry.clear();
        m_telemetry = source.m_telemetry;
    }
    return (*this);
}

const std::string& TrackedWSPR::get_name () const {
    return (this->m_tx_sign);
}

void TrackedWSPR::serialize(std::ostream& output) {
    char tx_sign[32];
    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    strncpy (tx_sign, m_tx_sign.c_str(), 30);
    tx_sign[31]=0;
    output.write(tx_sign, 32);
    output.write(reinterpret_cast<const char*>(&m_band), sizeof(m_band));
    output.write(reinterpret_cast<const char*>(&m_start_time), sizeof(time_t));
    uint32_t n_points = static_cast<uint32_t>(m_telemetry.size());                                     // need to investigate moving this to uint64
    output.write(reinterpret_cast<const char*>(&n_points), sizeof(n_points));
    if (n_points) {
         output.write(reinterpret_cast<const char*>(m_telemetry.data()),
                           n_points * sizeof(TrackedWSPR::WSPRTelemetry));
    }
    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
    return;
}

const struct GeoCoord TrackedWSPR::location () const {
    // get the current lat/lon over which the satellite currently is
    struct GeoCoord result = {0.0,0.0};
    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    if (!m_telemetry.empty()) {
        result =  m_telemetry.back().tx_loc;
    }
    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
    return result;
}

void TrackedWSPR::save_cache() {
    std::ostringstream cache_stream(std::ios::binary);
    cache_stream.clear();
    for (const auto& entry : m_telemetry) {
        cache_stream.write (reinterpret_cast<const char*>(&entry), sizeof(entry));
    }
    std::string cache_string;
    cache_string = cache_stream.str();
    std::fstream disk_file;
    disk_file.open((this->m_tx_sign+std::to_string(static_cast<int>(m_band))+".wspr"), (std::fstream::binary | std::fstream::out ));
    if (disk_file.is_open()) {
        disk_file.write(cache_string.data(), cache_string.size());
            // read cache from disk
    }
    disk_file.close();

}

void TrackedWSPR::load_new_telemetry(std::istream& input) {
    std::string input_line;
    while (std::getline(input, input_line)) {
        std::vector<std::string> fields;
        fields.clear();
        size_t start_index = 0;
        size_t end_index =0;
        while (start_index != std::string::npos) {
            end_index = input_line.find('\t', start_index);
            fields.push_back(input_line.substr(start_index, end_index - start_index));
            start_index = end_index;
            if (start_index != std::string::npos) {
                start_index++;
            }
        }
        if (fields.size() >=16) {
            struct WSPRTelemetry datapoint;
            try {
                datapoint.id = std::stoull(fields[0]);
                // convert raw_time to time_t here for timestamp
                struct tm new_time {};

                if (sscanf(fields[1].c_str(), "%4d-%2d-%2d %2d:%2d:%2d",
                &(new_time.tm_year), &(new_time.tm_mon), &(new_time.tm_mday),
                &(new_time.tm_hour), &(new_time.tm_min), &(new_time.tm_sec)) !=6) {
                    debug_log << "WSPR: Time parse error " <<  fields[1].c_str() << "\n";
                } else {
                    new_time.tm_year -=1900;
                    new_time.tm_mon--;
                    }
                datapoint.timestamp = 0;
                datapoint.timestamp = timegm(&new_time);
                debug_log << "WSPR: Input RX Latitude: " << fields[4].c_str();
                datapoint.rx_loc.latitude = std::stod(fields[4]);
                debug_log << " Input RX Long: " << fields[5].c_str() << "\n";
                datapoint.rx_loc.longitude = std::stod(fields[5]);
                datapoint.tx_loc.latitude = std::stod(fields[8]);
                datapoint.tx_loc.longitude = std::stod(fields[9]);
                strncpy (datapoint.rx_sign, fields[3].c_str(),31);
                strncpy (datapoint.tx_grid, fields[10].c_str(),8);
                strncpy (datapoint.rx_grid, fields[6].c_str(),8);
                datapoint.tx_power = std::stod(fields[15]);

                if (m_telemetry.empty() || (datapoint.timestamp > m_telemetry.back().timestamp)) {
                    m_telemetry.push_back(datapoint);
                }
            } catch (std::exception& e) {
                debug_log << "WSPR: Exception loading new telemetry: "<< e.what() << "\n";
            }
        }
    }
}

void TrackedWSPR::load_telemetry(std::istream& input) {
    struct WSPRTelemetry datapoint;
    while (input.read (reinterpret_cast<char*>(&datapoint), sizeof(datapoint))) {
        if (m_telemetry.empty() || (datapoint.timestamp > m_telemetry.back().timestamp)) {
            m_telemetry.push_back(datapoint);
        }
    }
    return;
}

void TrackedWSPR::wspr_live_update() {
    Uint64 data_size;
    void* http_buffer = nullptr;
    SDL_Log ("Checking for new data from db1.wspr.live");
    debug_log << "WSPR: Checking for new data from db1.wspr.live\n";
    std::string query = "SELECT * FROM wspr.rx WHERE tx_sign='"+m_tx_sign+"'";
    query += " AND band="+std::to_string(static_cast<int16_t>(m_band));
    if (!m_telemetry.empty()) {
        query += " AND id > " + std::to_string(m_telemetry.back().id);
    }
    const std::string url_string = "http://db1.wspr.live/?query="+url_encode(query);
    debug_log << "WSPR: Calling http loader with " << url_string.c_str() << "\n";;
    data_size = http_loader(url_string.c_str(), &http_buffer);   // live
    if (data_size) {
          // process raw new entries
        // update caches
        debug_log << "WSPR: Got new data from web\n";
        std::string data(reinterpret_cast<const char*>(http_buffer), data_size);
        std::istringstream stringbuffer(data);
        SDL_LockMutex(mutexes[MUTEX_WSPR]);
        load_new_telemetry(stringbuffer);
        save_cache();
        SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
        free (http_buffer);

    }
    return;
}


bool TrackedWSPR::check_cache (const std::string& data, std::string& telemetry_str) {
    struct head {
        char tx_sign[32];
        TrackedWSPR::Band band;
        time_t start_time;
        Uint32 telemetry_size;
    } header;
    std::istringstream stringbuffer(data);
    void* telemetry_data = 0;
    telemetry_str.clear();
    bool use_cache = false;
    while (stringbuffer.read (reinterpret_cast<char*>(&header), sizeof(header))) {
        // this allocates space for teh telemetry data we are about to read
        size_t telemetry_size = (header.telemetry_size * sizeof(TrackedWSPR::WSPRTelemetry));
        header.tx_sign[31] = 0;
        telemetry_data = malloc(telemetry_size);
        // pimary conditional if we have the right one or not
        if ((header.band == m_band) && (!strncmp(header.tx_sign, m_tx_sign.c_str(), 31))) {
            // we do here
            if (telemetry_data) {
                if (stringbuffer.read(reinterpret_cast<char*>(telemetry_data), telemetry_size)) {
                    debug_log << "WSPR: Got a cache Hit with "<<header.telemetry_size << " entries!\n";
                    telemetry_str.assign(reinterpret_cast<const char*>(telemetry_data), telemetry_size);
                    free (telemetry_data);
                    telemetry_data = nullptr;
                    use_cache = true;
                }
            }
        } else {
            // we don't here
            // seek past telemetry data here
            if (telemetry_data) {
                if (stringbuffer.read(reinterpret_cast<char*>(telemetry_data), telemetry_size)) {
                    free (telemetry_data);
                    telemetry_data = nullptr;
                }
            }
        }
    } // while
    return use_cache;
}

bool TrackedWSPR::gen_telemetry() {
    // generate the telemetry track for a WSPR station
    Uint64 data_size;
    bool add_flag;
    add_flag=true;
    void* data_buffer = nullptr;
    time_t cache_time;
    // check cache, then disk, then do a web query for anything new
    data_size = cache_loader(MOD_WSPR, &data_buffer, &cache_time);
    bool use_cache = false;
    std::istringstream telemetry_buffer;
    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    m_telemetry.clear();
    std::string telemetry_str;
    if (data_size) {
        // check for this WSPR station in cache and use it if found
        std::string data(reinterpret_cast<const char*>(data_buffer), data_size);
        use_cache = check_cache(data, telemetry_str);
	if (data_buffer) {
	    free (data_buffer);
	    data_buffer=nullptr;
	}
    }

    if (use_cache && !(telemetry_str.empty())) {
        debug_log << "WSPR: Using Cache data\n";
        telemetry_buffer.str(telemetry_str);
        load_telemetry(telemetry_buffer);
    } else {
        // read cache from disk
        debug_log << "WSPR: Trying Disk cache\n";
        cache_time = 0;
        std::fstream disk_file;
        disk_file.open((this->m_tx_sign+std::to_string(static_cast<int>(m_band))+".wspr"), (std::fstream::binary | std::fstream::in ));
        if (disk_file.is_open()) {
            load_telemetry(disk_file);
            if (!m_telemetry.empty()) {
                cache_time = m_telemetry.back().timestamp;
            }
        }
        disk_file.close();
    }
    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
//    SDL_Log ("Balloon spot count: %zu", m_telemetry.size());
    if ((time(NULL) - cache_time) > 1400) {
        wspr_live_update();
    }
    return add_flag;
}

time_t TrackedWSPR::telemetry_age() {
    time_t result = 0;
    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    if (m_telemetry.empty()) {
        result = m_telemetry.back().timestamp;
    }
    return result;
}





void TrackedWSPR::draw_telemetry(ScreenFrame& map) {
    // draw the satellite's telemetry track on the map
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("WSPR Draw during resize event!");
        return;
    }
    if (!map.GetRenderer()) {
        debug_log << "WSPR: Missing Renderer!\n";
        return;
    }
    if (!map.texture) {
        debug_log << "WSPR: Missing PANEL!\n";
        return;
    }

    if (this->m_telemetry.empty()) { return; }
    debug_log << "WSPR: Draw telemetry on texture: " << (void*)map.texture << "\n";
    SDL_SetRenderTarget(map.GetRenderer(), map.texture);
    SDL_SetRenderDrawBlendMode(map.GetRenderer(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(map.GetRenderer(), this->m_color.r, this->m_color.g, this->m_color.b, this->m_color.a);
    int index=0;
    int render_size=0;
    int xt, yt;
    xt = static_cast<int>(map.dims.w);
    yt = static_cast<int>(map.dims.h);

    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    SDL_FPoint* SDLPoints = (SDL_FPoint*)malloc(sizeof(SDL_FPoint)*this->m_telemetry.size());
    for (WSPRTelemetry point : m_telemetry) {
        cords_to_px(point.tx_loc.latitude, point.tx_loc.longitude, xt, yt, &(SDLPoints[index]));
        render_size++;
         SDL_SetRenderDrawColor(map.GetRenderer(), 128, 128, 255, 255);
         SDL_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 4.0, 4.0};
         SDL_RenderFillRect(map.GetRenderer(), &visirect);
         if (index > 1) {
             if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/8)) {

                  if (SDL_SetRenderDrawColor(map.GetRenderer(), this->m_color.r, this->m_color.g, this->m_color.b, this->m_color.a)) {
                      SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size-1);
                      index = 0;
                      render_size = 1;
                      // re-gen the current pixel
                      cords_to_px(point.tx_loc.latitude, point.tx_loc.longitude, xt, yt, &(SDLPoints[index]));
                  }

             }
         }
         index++;
    }
    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
    SDL_SetRenderDrawColor(map.GetRenderer(), this->m_color.r, this->m_color.g, this->m_color.b, this->m_color.a);
    SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size);
    free (SDLPoints);
    SDL_SetRenderTarget(map.GetRenderer(), NULL);
    return;
}


std::vector<TrackedWSPR> wsprlist;

void wspr_serialize() {
    std::ostringstream stringbuffer;
    for (TrackedWSPR& obj : wsprlist) {
        obj.serialize(stringbuffer);
    }
    std::string cache_string;
    cache_string = stringbuffer.str();
    add_data_cache(MOD_WSPR, cache_string.length(), (const void*)cache_string.data());

}

int SDLCALL update_wspr (void *userdata) {
    (void)userdata;
    SDL_Color wsprcolor;
    wsprcolor = {128, 128, 128, 255};
    std::string callsign;
    int band;
    while (clockconfig.next_wspr(&callsign, &band)) {
        wsprcolor.r += 16;
//        if (wsprcolor.r > 255) { wsprcolor.r=0; }
        wsprcolor.g -=16;
//        if (wsprcolor.g < 0) { wsprcolor.g=255; }
        wsprcolor.b -=16;
//        if (wsprcolor.b < 0) { wsprcolor.b=255; }
        wsprlist.emplace_back(callsign, static_cast<TrackedWSPR::Band>(band));
        wsprlist.back().gen_telemetry();
        wsprlist.back().m_color = wsprcolor;
    }
    return 0;
}

void wspr_tracker (ScreenFrame& panel, ScreenFrame& map) {
     if (wsprlist.empty()) {
          SDL_Thread* thread = SDL_CreateThread(update_wspr, "WSPR Init", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "Failed to Create WSPR Init Thread\n";
          }

     }
    delete_owner_pins(MOD_WSPR);
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("WSPR Tracker during resize event!");
        return;
    }
    if (!Sans) {
        debug_log << "WSPR: No font defined\n";
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "WSPR: Missing Renderer!\n";
        return;
    }
    if (!panel.texture) {
        debug_log << "WSPR: Missing PANEL!\n";
        return;
    }

    // clear the box
    panel.Clear();
    SDL_FRect mapsize ;
    mapsize.w = map.dims.w;
    mapsize.h = map.dims.h;


    debug_log << "WSPR: Drawing overlay\n";
    float height_unit = panel.dims.h / 20;
    float width_unit = panel.dims.w / 20;
    if (!wsprlist.empty()) {
        SDL_FRect TextBox ;
        char timestr[64];
        ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), MOD_WSPR, mapsize);
        overlay->Clear(SDL_Color{0,0,0,0});
        TextBox.x=width_unit;
        TextBox.y = height_unit;
        TextBox.h = height_unit*2;
        TextBox.w = width_unit*6;
        for (auto& new_wspr : wsprlist) {
            new_wspr.draw_telemetry(*overlay);
            if (TextBox.y < panel.dims.h) {
                TextBox.x=width_unit;
                TextBox.w = width_unit*6;
                panel.render_text(TextBox, Sans, new_wspr.m_color, new_wspr.get_name().c_str());
                TextBox.x = width_unit*10;
                TextBox.w = width_unit*9;
                time_t age = new_wspr.telemetry_age();
                struct tm* clocktime = gmtime(&age);
                strftime(timestr, sizeof(timestr), "%y-%m-%d %H:%M", clocktime);
                panel.render_text(TextBox, Sans, new_wspr.m_color, timestr);
                TextBox.y += height_unit*2;
            }
        }
    } else {
        panel.render_text(SDL_FRect{ width_unit, height_unit, width_unit * 18, height_unit*2 }, Sans, SDL_Color{ 255,200, 200, 255 }, "NO WSPR STATIONS");
        panel.render_text(SDL_FRect{ width_unit, height_unit*5, width_unit * 18, height_unit*2 }, Sans, SDL_Color{ 255,200, 200, 255 }, "CONFIGURED");
    }

    return;
}

