#include "aaediclock.h"
#include "utils.h"

#include "modules.h"
#include "classes.h"
#include <SDL3_image/SDL_image.h>

using json = nlohmann::json;


ScreenFrame::ScreenFrame() {
    if (this->valid()) {
        this->dims.x=0;
        this->dims.y=0;
        this->dims.h=0;
        this->dims.w=0;

        this->texture = 0;
        this->surface = 0;
        this->renderer = 0;
    }
}

ScreenFrame::~ScreenFrame() {
    if (valid()) {
        Reset();
    }
}

ScreenFrame::ScreenFrame(ScreenFrame&& source) noexcept {	// move to new instance
    if (valid() && source.valid()) {
        dims = std::move(source.dims);
        renderer = std::move(source.renderer);
        surface = source.surface;
        texture = source.texture;
        source.surface = nullptr;
        source.texture = nullptr;
        source.renderer = nullptr;
        source.dims = {};
    }
}

ScreenFrame& ScreenFrame::operator=(ScreenFrame&& source) noexcept {	// move over existing
    if (this != &source) {
        if (valid() && source.valid()) {
            this->Reset();
            dims = std::move(source.dims);
            renderer = std::move(source.renderer);
            surface = source.surface;
            texture = source.texture;
            source.surface = nullptr;
            source.texture = nullptr;
            source.renderer = nullptr;
            source.dims = {};
        }
    }
    return *this;
}

ScreenFrame::ScreenFrame(const ScreenFrame& source) {			// copy to new
    if (valid() && source.valid()) {
        dims = source.dims;
        panel_dims_check();
        renderer = source.renderer;
        surface=nullptr;
        texture=nullptr;
        if (source.surface) {
            surface = SDL_DuplicateSurface(source.surface);
             if (!surface) {
                SDL_Log("Failed to copy surface: %s", SDL_GetError());
                debug_log << "SCREENFRAME: Failed to copy surface: " << SDL_GetError() << "\n";
                // Handle error if needed
            }
        }
        if (renderer && surface) {
            debug_log << "SCREENFRAME: Attempting to create texture with renderer: "<< (void*)renderer << " and surface: " << (void*)surface << "\n";
            texture = SDL_CreateTextureFromSurface(renderer, surface);
    //        SDL_Log("texture Create result code: %s", SDL_GetError());
            if (!texture) {
                SDL_Log("Failed to create texture: %s", SDL_GetError());
                debug_log << "SCREENFRAME: Failed to create Texture: " << SDL_GetError() << "\n";
                // Handle error if needed
            }
        }
    }
}

ScreenFrame& ScreenFrame::operator=(const ScreenFrame& source) {	// copy with overwrite
    //SDL_Log ("Overwrite Copy Operation");
     if (this != &source) {
         if (valid() && source.valid()) {
           this->Reset();
           dims = source.dims;
           panel_dims_check();
           renderer = source.renderer;
           surface=nullptr;
           texture=nullptr;
           if (source.surface) {
               surface = SDL_DuplicateSurface(source.surface);
               if (!surface) {
                   SDL_Log("Failed to copy surface: %s", SDL_GetError());
                   debug_log << "SCREENFRAME: Failed to copy surface: " << SDL_GetError() << "\n";
               }
           }
           if (renderer && surface) {
               texture = SDL_CreateTextureFromSurface(renderer, surface);
               if (!texture) {
                   SDL_Log("Failed to create texture: %s", SDL_GetError());
                   debug_log << "SCREENFRAME: Failed to create Texture: " << SDL_GetError() << "\n";
               }
           } else {
               SDL_Log("Missing Render or Surface in Overwrite Copy");
               debug_log << "SCREENFRAME: Missing Render or Surface in Overwrite Copy\n";
           }
         }
     }
     return *this;
}

void ScreenFrame::panel_dims_check() {
    // prevent a texture size overflow if we ask for a texture larger than hardware supports
    if (max_tex_size < 10) {
        // if max size < 10, then it's invalid and we interpret as no limit. leave dims alone
        return;
    }
    if (dims.w > max_tex_size) {
        SDL_Log("Texture size limited by rendering engine or hardware");
        dims.w = max_tex_size;
    }
    if (dims.h > max_tex_size) {
        SDL_Log("Texture size limited by rendering engine or hardware");
        dims.h = max_tex_size;
    }
    return;
}


bool ScreenFrame::Create (SDL_Renderer* parent, const SDL_FRect size) {
    if (!valid()) {
        return false;
    }
    if (!parent) {
        debug_log << "SCREENFRAME: Bad Renderer passed to ScreenFrame Create!\n";
        return false;
    }
    dims=size;
    // limit panel dims to hardware max texture size if present
    panel_dims_check();
    int h = static_cast<int>(dims.h);
    int w = static_cast<int>(dims.w);
    if (w * h <= 0) {
        debug_log << "SCREENFRAME: Invalid Created Texture size! Returning FALSE (NULL TEXTURE)\n";
        Reset();
        return false;
    }

    SDL_SetRenderTarget(parent, nullptr);
    if (texture) {
        SDL_Renderer* texture_renderer = SDL_GetRendererFromTexture(this->texture);
        debug_log << "SCREENFRAME: Destroying " << ((dims.w*dims.h*4.0)/1024.0) << "KB 32 bit Texture " << dims.w << "x" << dims.h << "At " << (void*)texture << "\n";
        if (texture_renderer == parent) {
            SDL_DestroyTexture(texture);
        }
        texture = nullptr;
        renderer = nullptr;
    }

    texture = SDL_CreateTexture (parent, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                       w, h );
    if (!texture) {
        SDL_Log("Error Creating Texture!");
        debug_log << "SCREENFRAME: Error Creating Texture!\n";
        Reset();
        return false;
    }
    debug_log << "SCREENFRAME: Created " << ((w*h*4.0)/1024.0) << "KB 32 bit Texture " << w << "x" << h << "At " << (void*)texture << "\n";
    debug_log << "Texture Renderer" << (void*)SDL_GetRendererFromTexture(texture) << "\n";
    renderer=parent;
    Clear();
    return true;
}

SDL_Renderer* ScreenFrame::GetRenderer() {
    return renderer;
}

void ScreenFrame::SetRenderer(SDL_Renderer* source) {
    if (source) {
        renderer = source;
    }
    return;
}

void ScreenFrame::Reset() {
    if (!valid()) {
        return;
    }
    try {
        if (renderer) {
            SDL_SetRenderTarget(renderer, nullptr);
            if (this->texture) {
                SDL_Renderer* texture_renderer = SDL_GetRendererFromTexture(this->texture);
////                debug_log << "SCREENFRAME: Destroying " << ((dims.w * dims.h * 4.0) / 1024.0) << "KB 32 bit Texture " << dims.w << "x" << dims.h << "At " << (void*)texture << "\n";
//                std::cout << "SCREENFRAME: Destroying " << ((dims.w * dims.h * 4.0) / 1024.0) << "KB 32 bit Texture " << dims.w << "x" << dims.h << "At " << (void*)texture << "\n";
////                debug_log << "SCREENFRAME: Panel Address: " << (void*)this << "\n";
//                std::cout << "SCREENFRAME: Panel Address: " << (void*)this << "\n";
////                debug_log << "SCREENFRAME: Texture Renderer: " << (void*)texture_renderer;
//                std::cout << "SCREENFRAME: Texture Renderer: " << (void*)texture_renderer;
////                debug_log << " SCREENFRAME: ScreenFrame Renderer: " << (void*)(this->renderer) << "\n";
//                std::cout << " SCREENFRAME: ScreenFrame Renderer: " << (void*)(this->renderer) << "\n";
////                debug_log.flush();
                if (texture_renderer != renderer) {
//                    std::cout << "Texture Renderer mismatch on reset!\n";
                    debug_log << "SCREENFRAME: Texture Renderer mismatch on reset!\n";
                } else {
                    SDL_DestroyTexture(this->texture);
                }
            }
        }
    } catch (std::exception& e) {
        (void)e;
        debug_log << "Invalid Panel Texture\n";
    }
    try {
        if (this->surface) {
            debug_log << "SCREENFRAME: Destroying Surface " << dims.w << "x" << dims.h << "At " << (void*)this->surface << "\n";
            SDL_DestroySurface(this->surface);
        }
    } catch (std::exception& e) {
        (void)e;
        debug_log << "Invalid Panel Surface\n";
    }
    debug_log << "SCREENFRAME: Clearing ScreenFrame values\n";
    this->texture=nullptr;
    this->surface=nullptr;
    this->renderer = nullptr;
    this->dims = SDL_FRect{};
    return;
}

void ScreenFrame::draw_border() {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Draw Border during resize event!");
        return;
    }
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
        debug_log << "SCREENFRAME: Bad renderer or texture on border draw\n";
    }
    return;
}

void ScreenFrame::render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const std::string& str) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Text Draw during resize event!");
        return;
    }
    if (str.empty()) {
        // called with an empty string. Nothing to draw
        debug_log << "SCREENFRAME: Empty Text String\n";
        return;
    }
    if (text_box.w <= 0 || text_box.h <= 0) {
        debug_log << "SCREENFRAME: Text box wrong size!\n";
        return;
    }
    if (str.size() > 2048) {
        debug_log << "SCREENFRAME: Text Render input overflow. Trimmed\n";
        return;
    }
    if (!texture || !renderer || !font) {
        debug_log << "SCREENFRAME: Bad font, renderer or texture on Text Render\n";
        return;
    }


    SDL_Surface* textsurface = nullptr;
    SDL_Texture* TextTexture = nullptr;

    // render a text string
    textsurface = TTF_RenderText_Shaded(font, str.c_str(), str.size(), color, SDL_Color{0,0,0,0});
    if (textsurface==NULL) {
        debug_log << "SCREENFRAME: Text render error: " << SDL_GetError() << "\n";
        return;
    }
    TextTexture = SDL_CreateTextureFromSurface(renderer, textsurface);
    if (TextTexture) {
        SDL_SetRenderTarget(renderer, texture);
        SDL_RenderTexture(renderer, TextTexture, NULL, &text_box);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_DestroyTexture(TextTexture);
    } else {
        debug_log << "SCREENFRAME: Unable to render Text: " << SDL_GetError() << "\n";
    }
    SDL_DestroySurface(textsurface);
    return;
}


void ScreenFrame::render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const char* str) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Text Draw during resize event!");
        return;
    }
    if (text_box.w <= 0 || text_box.h <= 0) {
        debug_log << "SCREENFRAME: Text box wrong size!\n";
        return;
    }
    if (!texture || !renderer || !font) {
        debug_log << "SCREENFRAME: Bad font, renderer or texture on Text Render\n";
        return;
    }
    if (!str || !str[0]) {
        // called with an empty string. Nothing to draw
        debug_log << "SCREENFRAME: Empty Text String\n";
        return;
    }
    if (strlen(str)>2048) {
        debug_log << "SCREENFRAME: Text Render input overflow. Trimmed\n";
        return;
    }

    SDL_Surface* textsurface = nullptr;
    SDL_Texture* TextTexture = nullptr;

    // render a text string
    textsurface = TTF_RenderText_Shaded(font, str, strlen(str), color, SDL_Color{0,0,0,0});
    if (textsurface==NULL) {
        debug_log << "SCREENFRAME: Text render error: " << SDL_GetError() << "\n";
        return;
    }
    TextTexture = SDL_CreateTextureFromSurface(renderer, textsurface);
    if (TextTexture) {
        SDL_SetRenderTarget(renderer, texture);
        SDL_RenderTexture(renderer, TextTexture, NULL, &text_box);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_DestroyTexture(TextTexture);
    } else {
        debug_log << "SCREENFRAME: Unable to render Text: " << SDL_GetError() << "\n";
    }
    SDL_DestroySurface(textsurface);
    return;
}


void ScreenFrame::present() {
    if (renderer && texture && dims.w >0 && dims.h > 0 ) {
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderTexture(renderer, texture, NULL, &(dims));
    }
    return;
}

void ScreenFrame::Clear(const SDL_Color& color) {
    // clear the box
    SDL_ClearError();
    if (renderer && texture) {
        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);  // Clear solid
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderClear(renderer);  // Fills the entire target with the draw color
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);  // Clear solid
        SDL_SetRenderTarget(renderer, NULL);
    //    SDL_Log("SCREENFRAME: Clear Result %s", SDL_GetError());
        debug_log << "SCREENFRAME: Clear Result " << SDL_GetError()  << "\n";
        SDL_ClearError();
    } else {
        SDL_Log("Bad Renderer or Texture on Clear");
        debug_log << "SCREENFRAME: Bad renderer or texture on Clear\n";
    }
    return;
}

bool ScreenFrame::valid() const {
    return (magic == MAGIC_SCREENFRAME);
}

void config::qrz_sesskey() {
    char* xml = 0 ;
    Uint64 key_size =0;
    m_QRZ.Key.clear();
    if (!m_QRZ.Secret.empty()) {
        SDL_Log ("Fetching QRZ Session Key");
//    	debug_log << "CONFIF: Fetching QRZ Session Key\n";
        std::string url = "https://xmldata.qrz.com/xml/current/?username=" + m_CallSign + ";password=" + m_QRZ.Secret;
        key_size = http_loader(url.c_str(), (void**)&xml);
    }
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
//                debug_log << "CONFIG: Loaded QRZ Session Key\n";
            }

            tag_start=keyline.find("<Error>");
            tag_stop=keyline.find("</Error>");
            if (( tag_start != std::string::npos ) && ( tag_stop != std::string::npos)) {
                tag_start +=7;
                std::string QRZ_Err = keyline.substr(tag_start, tag_stop - tag_start);
                printf ("QRZ Session Key Error: %s\n", QRZ_Err.c_str());
//                debug_log << "CONFIG: QRZ Session Key Error: " << QRZ_Err.c_str() << "\n";
            }
        }

    }
    if (xml) {
        free(xml);
    }
    if (m_QRZ.Key.empty()) {
        printf ("Failed to load QRZ Session Key!\n");
//        debug_log << "CONFIG: Failed to load QRZ Session Key!\n";
    }
    return;
}

bool config::next_wspr(std::string *callsign, int *band) {
    if (m_WSPRList.empty()) {
        return false;
    }
    if (m_WSPRIndex < m_WSPRList.size()) {
        if (callsign && band) {
            *callsign = m_WSPRList[m_WSPRIndex].callsign;
            *band = m_WSPRList[m_WSPRIndex].band;
        }
        m_WSPRIndex++;
        return true;
    } else {
        m_WSPRIndex = 0;
        return false;
    }
}

void config::write_config() {
    json data = json({});
    data["CallSign"]=m_CallSign.c_str();
    if (!m_PSKCall.empty()) {
        data["PSKCall"]=m_PSKCall.c_str();
    }
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
    data["DX_Server"]["Name"] = m_dxserver.name;
    data["DX_Server"]["Port"] = m_dxserver.port;
    data["SatList"]=m_sats;
    data["Rss"]=m_rss;
    data["WSPR"]=nlohmann::json::array();
    for (const auto& entry : m_WSPRList) {
        data["WSPR"].push_back({
            {"callsign", entry.callsign},
            {"band", entry.band}
        });
    }
    std::ofstream f("aaediclock_config.json");
    if (!f) {
        SDL_Log("CONFIG: Failed to write configuration file!");
    }
    f << data.dump(5);
    f.close();
    return;
}

void config::load_config() {
    bool goodread = false;
    json data;

    m_CallSign = "N0CALL";
    m_PSKCall = "";
    m_DXMsg.clear();
    m_sats.clear();
    m_rss.clear();
    m_DE={0, 0};
    m_DX={0, 0};
    m_QRZ.Secret.clear();
    m_QRZ.Key.clear();
    m_dxserver.name = "dxfun.com";
    m_dxserver.port = 8000;

    printf ("Loading CONFIG\n");
//    debug_log << "CONFIG: Loading CONFIG\n";
    std::ifstream f("aaediclock_config.json");
    if (f.good()) {
        try {
            f >> data;          // parse the json
            goodread=true;
        } catch (const json::parse_error &e) {
            printf ("JSON parse error: %s\n",  e.what());
//            debug_log << "CONFIG: Config JSON parse error: " << e.what() << "\n";
            goodread=false;
        }
    } else {
        printf ("Config File Read error\n");
//        debug_log << "CONFIG: Config File Read error\n";
        goodread=false;
    }

    if (goodread) {
        if (data.contains("DE")) {
            if (data["DE"].contains("Latitude") && data["DE"].contains("Longitude")) {
                if (data["DE"]["Latitude"].is_number() && data["DE"]["Longitude"].is_number() ) {
                    m_DE.latitude=data["DE"]["Latitude"];
                    m_DE.longitude=data["DE"]["Longitude"];
                    if (m_DE.latitude < -90 || m_DE.latitude > 90) {
                        SDL_Log("CONFIG: DE Latitude out of range, resetting to 0");
                        m_DE.latitude = 0;
                    }
                    if (m_DE.longitude < -180 || m_DE.longitude > 180) {
                        SDL_Log("CONFIG: DE longitude out of range, resetting to 0");
                        m_DE.longitude = 0;
                    }

                }
            }
        }
        if (data.contains("DX")) {
            if (data["DX"].contains("Latitude") && data["DX"].contains("Longitude")) {
                if (data["DX"]["Latitude"].is_number() && data["DX"]["Longitude"].is_number() ) {
                    m_DX.latitude=data["DX"]["Latitude"];
                    m_DX.longitude=data["DX"]["Longitude"];
                    if (m_DX.latitude < -90 || m_DX.latitude > 90) {
                        SDL_Log("CONFIG: DX Latitude out of range, resetting to 0");
                        m_DX.latitude = 0;
                    }
                    if (m_DX.longitude < -180 || m_DX.longitude > 180) {
                        SDL_Log("CONFIG: DX longitude out of range, resetting to 0");
                        m_DX.longitude = 0;
                    }
                }
            }
        }

        if (data.contains("DX_Server")) {
            if (data["DX_Server"].contains("Name") && data["DX_Server"].contains("Port")) {
                if (data["DX_Server"]["Name"].is_string() && data["DX_Server"]["Port"].is_number()) {
                    m_dxserver.name= data["DX_Server"]["Name"];
                    m_dxserver.port = data["DX_Server"]["Port"];
                    if ((m_dxserver.port <1) || (m_dxserver.port >= 65534)) {
                        SDL_Log("CONFIG: DX Cluster Port, resetting to 8000");
                        m_dxserver.port=8000;

                    }
                }
            }
        }


        if (data.contains("CallSign")) {
            if (data["CallSign"].is_string()) {
                m_CallSign=data["CallSign"];
                if (m_CallSign.size() > 32) m_CallSign.resize(32);
            }
        }

        if (data.contains("PSKCall")) {
            if (data["PSKCall"].is_string()) {
                m_PSKCall=data["PSKCall"];
                if (m_PSKCall.size() > 32) m_PSKCall.resize(32);
            }
        }

        m_WSPRList.clear();
        m_WSPRIndex = 0;
        if (data.contains("WSPR")) {
            if (data["WSPR"].is_array()) {
                for (const auto& entry : data["WSPR"]) {
                    if (entry.contains("callsign") && entry.contains("band") && entry["band"].is_number()) {
                        std::string cs = entry["callsign"].get<std::string>();
                        if (cs.size() > 32) cs.resize(32);
                        int bd = entry["band"].get<int>();
                        m_WSPRList.push_back({cs, bd});
                    }
                }
            }
        }

        if (data.contains("QRZ")) {
            try {
            if (data["QRZ"].is_string()) {
                m_QRZ.Secret=data["QRZ"];
                if (m_QRZ.Secret.size() > 512) m_QRZ.Secret.resize(512);
                qrz_sesskey();
            } else if (data["QRZ"].is_array()) {
                std::vector<std::uint8_t> QRZ_secret;
                QRZ_secret = data["QRZ"].get<std::vector<std::uint8_t>>();
                for (size_t i = 0; i < QRZ_secret.size(); ++i) {
                    QRZ_secret[i] ^= static_cast<uint8_t>(i);
                }
                m_QRZ.Secret = json::from_cbor(QRZ_secret).get<std::string>();
                if (m_QRZ.Secret.size() > 512) m_QRZ.Secret.resize(512);
                qrz_sesskey();
            }
            } catch (json::parse_error &e) {
                (void)e;
                printf ("Invalid QRZ Passowrd\n");
//                debug_log << "CONFIG: Invalid QRZ Passowrd\n";
            }
        }

        if (data.contains("SatList")) {
            if (data["SatList"].is_array()) {
                for (const auto& item : data["SatList"]) {
                    if (item.is_string()) {
                        m_sats.push_back(item.get<std::string>().substr(0,25));
                    }
                }
            }
        }
        if (data.contains("Rss")) {
            if (data["Rss"].is_array()) {
                for (const auto& item : data["Rss"]) {
                    if (item.is_string()) {
                        m_rss.push_back(item.get<std::string>().substr(0,255));
                    }
                }
            }
        }
    } else {
        printf ("ERROR Reading CONFIG. Defaults used\n");
//        debug_log << "CONFIG: ERROR Reading CONFIG. Defaults used\n";
    }
    return;
} // loadconfig

config::config() {
    load_config();
}

config::~config() {}

void config::set_qrz_pass(const std::string& newpass) {
    m_QRZ.Secret=newpass;
    if (m_QRZ.Secret.size() > 512) m_QRZ.Secret.resize(512);
    write_config();
}

const std::string& config::CallSign() const {
    return m_CallSign;
}

const std::string& config::PSKCall() const {
    return m_PSKCall;
}

const GeoCoord& config::DE() const {
    return m_DE;
}

const GeoCoord& config::DX() const {
    return m_DX;
}

void config::set_DX(const GeoCoord& target, const std::string msg) {
    m_DX = target;
    if (m_DX.latitude < -90 || m_DX.latitude > 90) {
        SDL_Log("CONFIG: DX Latitude out of range, resetting to 0");
        m_DX.latitude = 0;
    }
    if (m_DX.longitude < -180 || m_DX.longitude > 180) {
        SDL_Log("CONFIG: DX longitude out of range, resetting to 0");
        m_DX.longitude = 0;
    }
    m_DXMsg = msg;
    return;
}

const std::string& config::DXmsg() const {
    return m_DXMsg;
}

const config::ip_server_t& config::dxserver() const {
    return m_dxserver;
}

const std::vector<std::string>& config::Sats() const {
    return m_sats;
}

const std::vector<std::string>& config::Rss() const {
    return m_rss;
}

const std::string& config::qrz_key(bool refresh) {
    if (refresh) {
        qrz_sesskey();
    }
    return m_QRZ.Key;
}

map_overlay::map_overlay () {
    index = 0;
    return;
}

map_overlay::~map_overlay() {
    for (auto& x : overlay_list) {
        x.panel.Reset();
    }
    overlay_list.clear();
    return;
}

void map_overlay::clear() {
    for (auto& x : overlay_list) {
        x.panel.Reset();
    }
    overlay_list.clear();
    index = 0;
    return;
}
ScreenFrame* map_overlay::get_overlay(SDL_Renderer* renderer, enum mod_name owner, SDL_FRect dims) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Overlay Fetch during resize event!");
        return (nullptr);
    }
    debug_log << "OVERLAY: Fetching Overlay : Current renderer pointer: " << (void*)renderer << "\n";
    for (auto& overlay : overlay_list) {
        if (overlay.owner == owner) {
            return &(overlay.panel);
        }
    }
    if (renderer) {
        if ((dims.w <= 0)||(dims.h <=0)) {
            debug_log << "OVERLAY: Invalid Overlay size\n";
            return nullptr;
        }
        struct transparancy new_overlay;
        new_overlay.owner = owner;
        new_overlay.panel.Create(renderer, dims);
        if (!new_overlay.panel.texture) {
            SDL_Log("Failed to create overlay texture: %s", SDL_GetError());
            debug_log << "OVERLAY: Failed to create overlay texture: " << SDL_GetError() << "\n";
            return nullptr;
        } else {
            debug_log << "OVERLAY: Created new overlay texture for module "<< owner << "\n";
        }
//        SDL_Log ("Created overlay ... %p\t Tex: %p", (void*)&(new_overlay.panel), (void*)(new_overlay.panel.texture));
        debug_log << "OVERLAY: Created "<< dims.w << "x" << dims.h << " " << ((dims.w*dims.h*4.0)/1024.0) << "KB overlay ... "<< (void*)&(new_overlay.panel) <<"\t Tex: " << (void*)(new_overlay.panel.texture)<< "\n";
        new_overlay.panel.Clear(SDL_Color{0,0,0,255});
        SDL_SetTextureBlendMode(new_overlay.panel.texture, SDL_BLENDMODE_BLEND);
        overlay_list.push_back(std::move(new_overlay));
        return (&(overlay_list.back().panel));
    }
    return nullptr;
}
bool map_overlay::overlay_check(enum mod_name owner) {
    for (auto& overlay : overlay_list) {
        if (overlay.owner == owner) {
            return true;
        }
    }
    return false;
}

ScreenFrame* map_overlay::next_overlay() {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Overlay call during resize event!");
        return (nullptr);
    }
    if (index < overlay_list.size()) {
        index++;
        debug_log << "OVERLAY: Returning Next panel: " << overlay_list[index-1].owner << "\n";
        return (&(overlay_list[index-1].panel));
    } else {
        index=0;
        return nullptr;
    }
}

void map_overlay::reset_index() {
    index=0;
    return;
}

void map_overlay::remove_overlay(enum mod_name owner) {
    for (auto it = overlay_list.begin(); it != overlay_list.end(); ++it) {
        if (it->owner == owner) {
            debug_log << "OVERLAY: Removing overlay for " << owner << "\n";
            it->panel.Reset();
            overlay_list.erase(it);
            return;
        }
    }
    return;
}

map_icons::map_icons (SDL_Renderer* renderer) {
    if (renderer) {
        reload_icons(renderer);
    }
}
map_icons::~map_icons() {
    clear_icons();
}
void map_icons::clear_icons() {
    debug_log << "ICONS: Clearing all icon textures\n";
    for (SDL_Texture*& tex : icons) {
        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
    }
}

void map_icons::set_dynamic(SDL_Renderer* renderer, SDL_Surface* source, enum icon_names id) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Dynamic ICON set during resize event!");
        return;
    }
    if (renderer && source) {
        if (icons[id]) {
            debug_log << "ICONS: destroying icon texture" << id << "\n";
            SDL_DestroyTexture(icons[id]);
            icons[id]=nullptr;
        }
        debug_log << "ICONS: Creating icon texture" << id << "\n";
        icons[id] = SDL_CreateTextureFromSurface(renderer, source);
        if (!icons[id]) {
            SDL_Log ("Unable to create dynamic icon");
            debug_log << "ICONS: Unable to create dynamic icon\n";
        } else {
            int w = source->w;
            int h = source->h;
            int bpp = 4;
            double surf_size_kb = (source->pitch * source->h) / 1024.0;
            double tex_size_kb = (w * h * 4.0) / 1024.0; // assuming RGBA8888
            debug_log << "ICONS: Created icon texture[" << id << "] "
              << w << "x" << h << " "
              << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
              << "=> GPU texture ≈ " << tex_size_kb << " KB "
              << "at " << static_cast<void*>(icons[id]) << "\n";
        }
    } else {
        SDL_Log ("Missing Renderer or Source for Dynamic Icon");
        debug_log << "ICONS: Missing Renderer or Source for Dynamic Icon\n";
    }
    return;
}

void map_icons::load_texture (SDL_Renderer* renderer, const std::string& path, const enum icon_names index) {
    icons[index]=nullptr;
    if (renderer) {
//      SDL_Surface* loadsurface = SDL_LoadBMP(path.c_str());
        SDL_Surface* loadsurface = IMG_Load(path.c_str());
        if (loadsurface) {
            if (icons[index]) {
                debug_log << "ICONS: destroying icon texture" << index << "\n";
                SDL_DestroyTexture(icons[index]);
                icons[index]=nullptr;
            }
            debug_log << "ICONS: Creating icon texture" << index << "\n";
            icons[index] = SDL_CreateTextureFromSurface(renderer, loadsurface);
            if (!icons[index]) {
                SDL_Log("ICONS: Unable to generate Icon Texture from %s", path.c_str());
                SDL_DestroySurface(loadsurface);
                return;
            } else {
                int w = loadsurface->w;
                int h = loadsurface->h;
                int bpp = 4;
                double surf_size_kb = (loadsurface->pitch * loadsurface->h) / 1024.0;
                double tex_size_kb = (w * h * 4.0) / 1024.0; // assuming RGBA8888
                debug_log << "ICONS: Created icon texture[" << index << "] "
                        << w << "x" << h << " "
                        << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
                        << "=> GPU texture ≈ " << tex_size_kb << " KB "
                        << "at " << static_cast<void*>(icons[index]) << "\n";
                SDL_DestroySurface(loadsurface);
            }
        } else {
            SDL_Log("Unable to load icon texture: %s", path.c_str());
            debug_log << "ICONS: Unable to load icon texture: " << path.c_str() << "\n";
            return;
        }

    } else {
        SDL_Log("No Icon load renderer provided!");
        debug_log << "ICONS: No Icon load renderer provided!\n";
        return;
    }
    return;
}
void map_icons::reload_icons(SDL_Renderer* renderer) {
    if (renderer) {
        clear_icons();
//        load_texture(renderer, "images/satellite.bmp", map_icons::ICON_SAT);
        load_texture(renderer, "images/satellite.png", map_icons::ICON_SAT);
    }
    return;
}
SDL_Texture* map_icons::get_icon(const enum icon_names index) {
    if ((index < 0) || (index > icons.size())) {
        return nullptr;
    }
    return (icons[index]);
}
