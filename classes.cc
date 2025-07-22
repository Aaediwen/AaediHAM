#include "aaediclock.h"
#include "utils.h"
#include "modules.h"
//#include <sstream>
//#include "json.hpp"
//#include <libsgp4/CoordTopocentric.h>

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


void ScreenFrame::present() {
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderTexture(renderer, texture, NULL, &(dims));
    return;
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
