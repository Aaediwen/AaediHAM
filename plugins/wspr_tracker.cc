

//#include "core/utils.h"
#include "utils/http_fetch.h"
#include "wspr_tracker.h"
#include "utils/conversions.h"

#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#endif
#include <cstring>
#include <sstream>
#include <fstream>
#include <mutex>

std::mutex wspr_mutex;
aaediclock_host_api* host_api = nullptr;
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
//    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    const std::lock_guard<std::mutex>wspr_lock(wspr_mutex);
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
//    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
    return;
}

const struct GeoCoord TrackedWSPR::location () const {
    // get the current lat/lon over which the satellite currently is
    struct GeoCoord result = {0.0,0.0};
//    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    const std::lock_guard<std::mutex>wspr_lock(wspr_mutex);
    if (!m_telemetry.empty()) {
        result =  m_telemetry.back().tx_loc;
    }
//    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
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
    *(host_api->AaediHAM_LogDebug) << "Loading new telemetry from input stream\n";
    const std::lock_guard<std::mutex>wspr_lock(wspr_mutex);
    while (std::getline(input, input_line)) {
        *(host_api->AaediHAM_LogDebug) << "Read input line: "<< input_line <<" : \n";
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
        *(host_api->AaediHAM_LogDebug) << "Entry has " << fields.size() << "Field entries\n";
        if (fields.size() >=16) {
            struct WSPRTelemetry datapoint;
            try {
                datapoint.id = std::stoull(fields[0]);
                // convert raw_time to time_t here for timestamp
                struct tm new_time {};

                if (sscanf(fields[1].c_str(), "%4d-%2d-%2d %2d:%2d:%2d",
                &(new_time.tm_year), &(new_time.tm_mon), &(new_time.tm_mday),
                &(new_time.tm_hour), &(new_time.tm_min), &(new_time.tm_sec)) !=6) {
                    *(host_api->AaediHAM_LogDebug) << "WSPR: Time parse error " <<  fields[1].c_str() << "\n";
                } else {
                    new_time.tm_year -=1900;
                    new_time.tm_mon--;
                    }
                datapoint.timestamp = 0;
                datapoint.timestamp = timegm(&new_time);
                *(host_api->AaediHAM_LogDebug) << "WSPR: Input RX Latitude: " << fields[4].c_str();
                datapoint.rx_loc.latitude = std::stod(fields[4]);
                *(host_api->AaediHAM_LogDebug) << " Input RX Long: " << fields[5].c_str() << "\n";
                datapoint.rx_loc.longitude = std::stod(fields[5]);
                datapoint.tx_loc.latitude = std::stod(fields[8]);
                datapoint.tx_loc.longitude = std::stod(fields[9]);
                strncpy (datapoint.rx_sign, fields[3].c_str(),31);
                strncpy (datapoint.tx_grid, fields[10].c_str(),8);
                strncpy (datapoint.rx_grid, fields[6].c_str(),8);
                datapoint.tx_power = std::stod(fields[15]);
                *(host_api->AaediHAM_LogDebug) << "prepared telemetry entry \n";
                if (m_telemetry.empty() || (datapoint.timestamp > m_telemetry.back().timestamp)) {
                    m_telemetry.push_back(datapoint);
                    *(host_api->AaediHAM_LogDebug) << "Added telemetry entry \n";
                }
            } catch (std::exception& e) {
                *(host_api->AaediHAM_LogDebug) << "WSPR: Exception loading new telemetry: "<< e.what() << "\n";
            }
        }
    }
    *(host_api->AaediHAM_LogDebug) << "loaded "<< m_telemetry.size() << " telemetry entries\n";
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
    *(host_api->AaediHAM_LogDebug) << "WSPR: Checking for new data from db1.wspr.live\n";
    std::string query = "SELECT * FROM wspr.rx WHERE tx_sign='"+m_tx_sign+"'";
    query += " AND band="+std::to_string(static_cast<int16_t>(m_band));
    if (!m_telemetry.empty()) {
        query += " AND id > " + std::to_string(m_telemetry.back().id);
    }
    const std::string url_string = "http://db1.wspr.live/?query="+query;
    *(host_api->AaediHAM_LogDebug) << "WSPR: Calling http loader with " << url_string.c_str() << "\n";;
    data_size = http_loader(url_string.c_str(), &http_buffer);   // live
    if (data_size) {
          // process raw new entries
        // update caches
        *(host_api->AaediHAM_LogDebug) << "WSPR: Got new data from web\n";
        std::string data(reinterpret_cast<const char*>(http_buffer), static_cast<size_t>(data_size));
        std::istringstream stringbuffer(data);
//        SDL_LockMutex(mutexes[MUTEX_WSPR]);
        *(host_api->AaediHAM_LogDebug) << "WSPR: Prepared buffer, calling load new telemetry\n";
        load_new_telemetry(stringbuffer);
        *(host_api->AaediHAM_LogDebug) << "WSPR: loaded new telemetry, saving cache\n";
        save_cache();
        *(host_api->AaediHAM_LogDebug) << "WSPR: disk cache written \n";
//        SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
        free (http_buffer);

    } else {
        *(host_api->AaediHAM_LogDebug) << "No new data from WSPR Live\n";
    }
    return;
}

bool TrackedWSPR::gen_telemetry() {
    // generate the telemetry track for a WSPR station
    bool add_flag;
    add_flag=true;
    time_t cache_time;
    // check cache, then disk, then do a web query for anything new
    bool use_cache = false;
    std::istringstream telemetry_buffer;
//    const std::lock_guard<std::mutex>wspr_lock(wspr_mutex);
    m_telemetry.clear();
    std::string telemetry_str;

        // read cache from disk
        *(host_api->AaediHAM_LogDebug) << "WSPR: Trying Disk cache\n";
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
//    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
//    SDL_Log ("Balloon spot count: %zu", m_telemetry.size());
    if ((time(NULL) - cache_time) > 1400) {
        wspr_live_update();
    }
    return add_flag;
}

time_t TrackedWSPR::telemetry_age() {
    time_t result = 0;
//    SDL_LockMutex(mutexes[MUTEX_WSPR]);
//    const std::lock_guard<std::mutex>wspr_lock(wspr_mutex);
    if (!m_telemetry.empty()) {
        result = m_telemetry.back().timestamp;
    }
    return result;
}





void TrackedWSPR::draw_telemetry(aaediclock_FRect dims) {
    // draw the satellite's telemetry track on the map
/*    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("WSPR Draw during resize event!");
        return;
    }
    if (!map.GetRenderer()) {
        *(host_api->AaediHAM_LogDebug) << "WSPR: Missing Renderer!\n";
        return;
    }
    if (!map.texture) {
        *(host_api->AaediHAM_LogDebug) << "WSPR: Missing PANEL!\n";
        return;
    }
*/
    if (this->m_telemetry.empty()) { return; }

//    SDL_SetRenderTarget(map.GetRenderer(), map.texture);
//    SDL_SetRenderDrawBlendMode(map.GetRenderer(), SDL_BLENDMODE_BLEND);
//    SDL_SetRenderDrawColor(map.GetRenderer(), this->m_color.r, this->m_color.g, this->m_color.b, this->m_color.a);
    int index=0;
    int render_size=0;
    int xt, yt;
    xt = static_cast<int>(dims.w);
    yt = static_cast<int>(dims.h);
    const std::lock_guard<std::mutex>wspr_lock(wspr_mutex);
//    SDL_LockMutex(mutexes[MUTEX_WSPR]);
    aaediclock_FPoint* SDLPoints = (aaediclock_FPoint*)malloc(sizeof(aaediclock_FPoint)*this->m_telemetry.size());
    *(host_api->AaediHAM_LogDebug) << "Drawing "<< m_telemetry.size() << " telemetry entries\n";
    for (WSPRTelemetry point : m_telemetry) {
        *(host_api->AaediHAM_LogDebug) << "Drawing TX from "<< point.tx_grid << ", ("<<point.tx_loc.latitude<<", "<< point.tx_loc.longitude << ") \n";
        cords_to_px(point.tx_loc.latitude, point.tx_loc.longitude, xt, yt, &(SDLPoints[index]));
        render_size++;
//         SDL_SetRenderDrawColor(map.GetRenderer(), 128, 128, 255, 255);
         aaediclock_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 4.0, 4.0};
         host_api->AaediHAM_OverlaySet(dims);
         host_api->AaediHAM_GraphicsDrawRect(aaediclock_Color{128,128,255,255},visirect,1);
//         SDL_RenderFillRect(map.GetRenderer(), &visirect);
         if (index > 1) {
             if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/8)) {
                  host_api->AaediHAM_GraphicsDrawLines(this->m_color, SDLPoints, render_size-1);
//                  if (SDL_SetRenderDrawColor(map.GetRenderer(), this->m_color.r, this->m_color.g, this->m_color.b, this->m_color.a)) {
//                      SDL_RenderLines(map.GetRenderer(), );
                      index = 0;
                      render_size = 1;
                      // re-gen the current pixel
                      cords_to_px(point.tx_loc.latitude, point.tx_loc.longitude, xt, yt, &(SDLPoints[index]));
//                  }

             }
         }
         index++;
    }
//    SDL_UnlockMutex(mutexes[MUTEX_WSPR]);
    host_api->AaediHAM_GraphicsDrawLines(this->m_color, SDLPoints, render_size);
//    SDL_SetRenderDrawColor(map.GetRenderer(), this->m_color.r, this->m_color.g, this->m_color.b, this->m_color.a);
//    SDL_RenderLines(map.GetRenderer(), SDLPoints, render_size);
    free (SDLPoints);
//    SDL_SetRenderTarget(map.GetRenderer(), NULL);
    return;
}


std::vector<TrackedWSPR> wsprlist;
/*
void wspr_serialize() {
    std::ostringstream stringbuffer;
    for (TrackedWSPR& obj : wsprlist) {
        obj.serialize(stringbuffer);
    }
    std::string cache_string;
    cache_string = stringbuffer.str();
    add_data_cache(MOD_WSPR, cache_string.length(), (const void*)cache_string.data());

}
*/
int SDLCALL update_wspr (void *userdata) {
    (void)userdata;
    aaediclock_Color wsprcolor;
    wsprcolor = {128, 128, 128, 255};
    std::string callsign;
//    int band;
    struct plugin_wspr_station  wspr_station =  host_api->AaediHAM_ConfigGetNextWspr();
    wsprlist.clear();
    while (wspr_station.callsign[0] !=0) {
      callsign = wspr_station.callsign;
//    while (clockconfig.next_wspr(&callsign, &band)) {
        wsprcolor.r += 16;
//        if (wsprcolor.r > 255) { wsprcolor.r=0; }
        wsprcolor.g -=16;
//        if (wsprcolor.g < 0) { wsprcolor.g=255; }
        wsprcolor.b -=16;
//        if (wsprcolor.b < 0) { wsprcolor.b=255; }
        *(host_api->AaediHAM_LogDebug) << "Adding WSPR Station: " << callsign << "\n";
        wsprlist.emplace_back(callsign, static_cast<TrackedWSPR::Band>(wspr_station.band));
        wsprlist.back().gen_telemetry();
        wsprlist.back().m_color = wsprcolor;
        wspr_station =  host_api->AaediHAM_ConfigGetNextWspr();
    }
    return 0;
}

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new wspr_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void wspr_plugin::plugin_init() const {
          SDL_Thread* thread = SDL_CreateThread(update_wspr, "WSPR Init", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              *(host_api->AaediHAM_LogDebug) << "Failed to Create WSPR Init Thread\n";
          }

    return;
}

void wspr_plugin::plugin_exit() const {
    return;
}

void wspr_plugin::plugin_main(const aaediclock_FRect& dims) const {

//    delete_owner_pins(MOD_WSPR);
    host_api->AaediHAM_MapPinDelete();

    // clear the box
    host_api->AaediHAM_GraphicsClear();

    aaediclock_FRect mapsize = host_api->AaediHAM_GetMapSize();

    *(host_api->AaediHAM_LogDebug) << "WSPR: Drawing overlay\n";
    float height_unit = dims.h / 20;
    float width_unit = dims.w / 20;
    if (!wsprlist.empty()) {
        aaediclock_FRect TextBox ;
        char timestr[64];
        host_api->AaediHAM_OverlaySet(mapsize);
        host_api->AaediHAM_OverlayClear(aaediclock_Color{0,0,0,0});
//        ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), MOD_WSPR, mapsize);
//        overlay->Clear(SDL_Color{0,0,0,0});
        TextBox.x=width_unit;
        TextBox.y = height_unit;
        TextBox.h = height_unit*2;
        TextBox.w = width_unit*6;
        for (auto& new_wspr : wsprlist) {
            *(host_api->AaediHAM_LogDebug) << "WSPR: Drawing overlay for "<< new_wspr.get_name()<<"  \n";
            new_wspr.draw_telemetry(mapsize);
            if (TextBox.y < dims.h) {
                TextBox.x=width_unit;
                TextBox.w = width_unit*6;
                host_api->AaediHAM_GraphicsDrawText(new_wspr.get_name().c_str(), new_wspr.m_color, TextBox);
//                panel.render_text(TextBox, Sans, new_wspr.m_color, new_wspr.get_name().c_str());
                TextBox.x = width_unit*10;
                TextBox.w = width_unit*9;
                time_t age = new_wspr.telemetry_age();
                struct tm* clocktime = gmtime(&age);
                strftime(timestr, sizeof(timestr), "%y-%m-%d %H:%M", clocktime);
                host_api->AaediHAM_GraphicsDrawText(timestr, new_wspr.m_color, TextBox);
//                panel.render_text(TextBox, Sans, new_wspr.m_color, timestr);
                TextBox.y += height_unit*2;
            }
        }
    } else {
        aaediclock_FRect TextBox ;
        TextBox.x = width_unit;
        TextBox.y = height_unit;
        TextBox.w = width_unit * 18;
        TextBox.h = height_unit*2;
        host_api->AaediHAM_GraphicsDrawText("NO WSPR STATIONS", aaediclock_Color{255, 200, 200, 255}, TextBox);
        TextBox.y = height_unit * 18;
        host_api->AaediHAM_GraphicsDrawText("CONFIGURED", aaediclock_Color{255, 200, 200, 255}, TextBox);
    }

    return;
}

const char* wspr_plugin::getName() const {
    return "WSPR Module";
}

void wspr_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

