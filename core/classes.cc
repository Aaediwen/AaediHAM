#include "aaediclock.h"
//#include "utils.h"
#include "utils/http_fetch.h"
#include <fstream>
#include <sstream>
//#include "modules.h"
#include "classes.h"
#include <libxml/tree.h>
#include <nlohmann/json.hpp>
#include <SDL3_image/SDL_image.h>

using		json	= nlohmann::json;
config		clockconfig;
map_overlay	overlays;
map_icons	icon_bin;

ScreenFrame::ScreenFrame() {
	if (this->valid()) {
		this->dims.x	= 0;
		this->dims.y	= 0;
		this->dims.h	= 0;
		this->dims.w	= 0;

		this->texture	= 0;
		this->surface	= 0;
		this->renderer	= 0;
	}
}

ScreenFrame::~ScreenFrame() {
	if (valid()) {
		Reset();
	}
}

ScreenFrame::ScreenFrame(ScreenFrame&& source) noexcept {	// move to new instance
	if (valid()) {
		dims		= {};
		texture		= nullptr;
		surface		= nullptr;
		renderer	= nullptr;
	}
	if (valid() && source.valid()) {
		dims		= std::move(source.dims);
		renderer	= std::move(source.renderer);
		surface		= source.surface;
		texture		= source.texture;
		source.surface	= nullptr;
		source.texture	= nullptr;
		source.renderer	= nullptr;
		source.dims	= {};
	}
}

ScreenFrame& ScreenFrame::operator=(ScreenFrame&& source) noexcept {	// move over existing
	if (this != &source) {
		if (valid() && source.valid()) {
			this->Reset();
			dims		= std::move(source.dims);
			renderer	= std::move(source.renderer);
			surface		= source.surface;
			texture		= source.texture;
			source.surface	= nullptr;
			source.texture	= nullptr;
			source.renderer	= nullptr;
			source.dims	= {};
		}
	}
	return *this;
}

ScreenFrame::ScreenFrame(const ScreenFrame& source) {			// copy to new
	if (valid()) {
		dims			= {};
		texture			= nullptr;
		surface			= nullptr;
		renderer		= nullptr;
	}
	if (valid() && source.valid()) {
		dims			= source.dims;
		panel_dims_check();
		renderer		= source.renderer;
		surface			= nullptr;
		texture			= nullptr;
		if (source.surface) {
			surface		= SDL_DuplicateSurface(source.surface);
			if (!surface) {
				SDL_Log("Failed to copy surface: %s", SDL_GetError());
				debug_log << "SCREENFRAME: Failed to copy surface: " << SDL_GetError() << "\n";
        			// Handle error if needed
			}
		}
		if (renderer && surface) {
			debug_log << "SCREENFRAME: Attempting to create texture with renderer: "<< (void*)renderer 
				<< " and surface: " << (void*)surface << "\n";
			texture			= SDL_CreateTextureFromSurface(renderer, surface);
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
			dims		= source.dims;
			panel_dims_check();
			renderer	= source.renderer;
			surface		= nullptr;
			texture		= nullptr;
			if (source.surface) {
				surface	= SDL_DuplicateSurface(source.surface);
				if (!surface) {
					SDL_Log("Failed to copy surface: %s", SDL_GetError());
					debug_log << "SCREENFRAME: Failed to copy surface: " << SDL_GetError() << "\n";
				}
			}
			if (renderer && surface) {
				texture	= SDL_CreateTextureFromSurface(renderer, surface);
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
		dims.w = static_cast<float>(max_tex_size);
	}
	if (dims.h > max_tex_size) {
		SDL_Log("Texture size limited by rendering engine or hardware");
		dims.h = static_cast<float>(max_tex_size);
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
		debug_log << "SCREENFRAME: Destroying " << ((dims.w*dims.h*4.0)/1024.0) << "KB 32 bit Texture " 
			<< dims.w << "x" << dims.h << "At " << (void*)texture << "\n";
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
	renderer = parent;
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
				if (texture_renderer != renderer) {
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
			debug_log << "SCREENFRAME: Destroying Surface " << dims.w << "x" << dims.h 
				<< "At " << (void*)this->surface << "\n";
			SDL_DestroySurface(this->surface);
		}
	} catch (std::exception& e) {
		(void)e;
		debug_log << "Invalid Panel Surface\n";
	}
	debug_log << "SCREENFRAME: Clearing ScreenFrame values\n";
	this->texture	= nullptr;
	this->surface	= nullptr;
	this->renderer	= nullptr;
	this->dims	= SDL_FRect{};
	return;
}

void ScreenFrame::draw_border() {
	if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
		SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
	} else {
		SDL_Log("Draw Border during resize event!");
		return;
	}
	if (texture && renderer) {
		SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
		SDL_FRect	border;
		border.x	= 0;
		border.y	= 0;
		border.w	= dims.w;
		border.h	= dims.h;
		SDL_SetRenderTarget(renderer, texture);
		SDL_RenderRect(renderer, &(border));
		SDL_SetRenderTarget(renderer, NULL);
		SDL_RenderTexture(renderer, texture, NULL, &(dims));
	} else {
		SDL_Log("Bad renderer or texture on border draw");
		debug_log << "SCREENFRAME: Bad renderer or texture on border draw\n";
	}
	return;
}

void ScreenFrame::render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const std::string& str) {
	if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
		SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
	} else {
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
		debug_log << "SCREENFRAME: Text Render input overflow. Discarded\n";
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
	} else {
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
		debug_log << "SCREENFRAME: Text Render input overflow. Discarded\n";
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
	//      SDL_SetRenderTarget(renderer, NULL);
	//      SDL_Log("SCREENFRAME: Clear Result %s", SDL_GetError());
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

void config::parse_qrz(void* node) {
	xmlNode* start_node = static_cast<xmlNode*>(node);
	xmlNode* current_node = nullptr;
	for (current_node = start_node; current_node; current_node = current_node->next) {
		if (current_node->type == XML_ELEMENT_NODE) {
			std::string NodeName(reinterpret_cast<const char*>(current_node->name));
			//std::cout << "QRZ XML Node Name: "<< NodeName << "\n";
			std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
			if ((NodeName == "qrzdatabase")||(NodeName == "session")) {
				parse_qrz(current_node->children);
			} else if (NodeName == "key") {
				std::string content;
				content.clear();
				// extract the node content
				xmlChar* raw = xmlNodeGetContent(current_node);
				if (raw) {
					content = reinterpret_cast<char*>(raw);
					m_QRZ.Key = content;
					xmlFree(raw);
					// attempt to clean up HTML
				}
			} else if (NodeName == "error") {
				xmlChar* raw = xmlNodeGetContent(current_node);
				if (raw) {
					std::string QRZ_Err =  reinterpret_cast<char*>(raw);
					printf ("QRZ Session Key Error: %s\n", QRZ_Err.c_str());
					xmlFree(raw);
				}
			}
		} // XML_ELEMENT_NODE
	}
	return;
}

void config::qrz_sesskey() {
	char* xml = 0 ;
	Uint64 key_size =0;
	m_QRZ.Key.clear();
	if (!m_QRZ.Secret.empty()) {
		SDL_Log ("Fetching QRZ Session Key");
		std::string url = "https://xmldata.qrz.com/xml/current/?username=" + m_CallSign + ";password=" + m_QRZ.Secret;
		key_size = http_loader(url.c_str(), (void**)&xml);
	}
	if (key_size) {
		// parse XML for session key
		xmlDocPtr xml_tree = 0;
		debug_log << "Calling XML ReadMemory\n";
		xml_tree = xmlReadMemory(xml, static_cast<int>(key_size), nullptr, nullptr, 0);
		if (!xml_tree) {
			debug_log << "RSS: Failed to parse QRZ Feed XML\n";
		} else {
			parse_qrz(xmlDocGetRootElement(xml_tree));
			xmlFreeDoc (xml_tree);
		}
	}
	if (xml) {
		free(xml);
	}
	if (m_QRZ.Key.empty()) {
		printf ("Failed to load QRZ Session Key!\n");
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

config::plugin config::next_plugin() {
	config::plugin result;
	result.filename.clear();
	if (m_Plugins.empty()) {
		return result;
	}
	if (m_PluginIndex < m_Plugins.size()) {
		result = m_Plugins[m_PluginIndex];
		m_PluginIndex++;
		return result;
	} else {
		m_PluginIndex = 0;
		return result;
	}
}

void config::write_config() {
	json data			= json({});
	data["CallSign"]		= m_CallSign.c_str();
	if (!m_PSKCall.empty()) {
		data["PSKCall"]		= m_PSKCall.c_str();
	}
	if (m_plugin_path.empty()) {
		data["PluginPath"]	= "plugins";
	} else {
		data["PluginPath"]	= m_plugin_path;
	}
	if (!m_site_cache.empty()) {
		data["SiteCacheServer"]	= m_site_cache;
	}
	if (m_asset_path.empty()) {
		data["AssetPath"]	= "images";
	} else {
		data["AssetPath"]	= m_asset_path;
	}
	data["DE"]["Latitude"]		= m_DE.latitude;
	data["DE"]["Longitude"]		= m_DE.longitude;
	data["DX"]["Latitude"]		= m_DX.latitude;
	data["DX"]["Longitude"]		= m_DX.longitude;
	std::vector<std::uint8_t>	QRZ_secret;
	QRZ_secret			= json::to_cbor(m_QRZ.Secret);
	for (size_t i = 0; i < QRZ_secret.size(); ++i) {
		QRZ_secret[i]		^= static_cast<uint8_t>(i);
	}
	data["QRZ"]			= QRZ_secret;
	data["DX_Server"]["Name"]	= m_dxserver.name;
	data["DX_Server"]["Port"]	= m_dxserver.port;
	data["SatList"]			= m_sats;
	data["Rss"]			= m_rss;
	data["WSPR"]			= nlohmann::json::array();
	for (const auto& entry : m_WSPRList) {
		data["WSPR"].push_back({
			{"callsign", entry.callsign},
			{"band", entry.band}
		});
	}
	data["Plugins"]=nlohmann::json::array();
	for (const auto& entry : m_Plugins) {
		data["Plugins"].push_back({
			{"plugin", entry.filename},
			{"panel", entry.panel_id},
			{"interval", entry.interval}
		});
	}
	std::ofstream f("aaediclock_config.json");
	if (!f) {
		SDL_Log("CONFIG: Failed to write configuration file!");
	} else {
		f << data.dump(5);
		f.close();
	}
	return;
}

std::string config::path_seperator(std::string& input) {
	if (input.empty()) {
		return "";
	}
	std::string seperator;
	#ifdef _WIN32
	seperator = "\\";
	#else
	seperator = "/";
	#endif
	std::string result = input;
	if (result.back() != seperator.back()) {
		result += seperator;
	}
	return result;
}

void config::reload(const std::string filename) {
	load_config(filename);
}
void config::load_config(const std::string filename) {
	bool 	goodread	= false;
	json 	data;
	
	m_CallSign		= "N0CALL";
	m_PSKCall		= "";
	m_DXMsg.clear();
	m_sats.clear();
	m_rss.clear();
	m_Plugins.clear();
	m_PluginIndex		= 0;
	m_plugin_path		= ".";
	m_cache_path		= ".";
	m_site_cache.clear();
	m_asset_path		= "images";
	m_DE			= {0, 0};
	m_DX			= {0, 0};
	m_QRZ.Secret.clear();
	m_QRZ.Key.clear();
	m_dxserver.name		= "dxfun.com";
	m_dxserver.port		= 8000;
	if (filename.empty()) {
		return;
	}
	printf ("Loading CONFIG from %s\n", filename.c_str());
	//    debug_log << "CONFIG: Loading CONFIG\n";
	std::ifstream f(filename.c_str());
	if (f.good()) {
		try {
			f >> data;          // parse the json
			goodread=true;
		} catch (const json::parse_error &e) {
			printf ("JSON parse error: %s\n",  e.what());
			//    debug_log << "CONFIG: Config JSON parse error: " << e.what() << "\n";
			goodread = false;
		}
	} else {
		printf ("Config File Read error\n");
		//        debug_log << "CONFIG: Config File Read error\n";
		goodread = false;
	}

	if (goodread) {
		if (data.contains("DE")) {
			if (data["DE"].contains("Latitude") && data["DE"].contains("Longitude")) {
				if (data["DE"]["Latitude"].is_number() && data["DE"]["Longitude"].is_number() ) {
					m_DE.latitude = data["DE"]["Latitude"];
					m_DE.longitude = data["DE"]["Longitude"];
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
					m_DX.latitude = data["DX"]["Latitude"];
					m_DX.longitude = data["DX"]["Longitude"];
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
					m_dxserver.name = data["DX_Server"]["Name"];
					m_dxserver.port = data["DX_Server"]["Port"];
					if ((m_dxserver.port <1) || (m_dxserver.port >= 65534)) {
						SDL_Log("CONFIG: DX Cluster Port, resetting to 8000");
						m_dxserver.port = 8000;
					}
				}
			}
		}
	
		if (data.contains("PluginPath")) {
			if (data["PluginPath"].is_string()) {
				m_plugin_path = data["PluginPath"];
			}
		}
		m_plugin_path = path_seperator(m_plugin_path);
		if (data.contains("SiteCacheServer")) {
			if (data["SiteCacheServer"].is_string()) {
				m_site_cache = data["SiteCacheServer"];
			}
			if (!m_site_cache.empty() && m_site_cache.back() != '/') { 
				m_site_cache += '/';
			}

		}

		if (data.contains("AssetPath")) {
			if (data["AssetPath"].is_string()) {
				m_asset_path = data["AssetPath"];
			}
		}
		m_asset_path = path_seperator(m_asset_path);
		
		if (data.contains("CachePath")) {
			if (data["CachePath"].is_string()) {
				m_cache_path = data["CachePath"];
			}
		}
		m_cache_path = path_seperator(m_cache_path);
		
		
		if (data.contains("CallSign")) {
			if (data["CallSign"].is_string()) {
				m_CallSign = data["CallSign"];
				if (m_CallSign.size() > 32) m_CallSign.resize(32);
			}
		}
		
		if (data.contains("PSKCall")) {
			if (data["PSKCall"].is_string()) {
				m_PSKCall = data["PSKCall"];
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
		if (data.contains("Plugins")) {
			if (data["Plugins"].is_array()) {
				for (const auto& entry : data["Plugins"]) {
					if (entry.contains("plugin") && entry.contains("panel") 
						&& entry.contains("interval") && entry["panel"].is_number() 
						&& entry["interval"].is_number() && entry["plugin"].is_string()) {

						plugin new_plugin;
						new_plugin.filename = entry["plugin"].get<std::string>();
						new_plugin.panel_id = entry["panel"].get<size_t>();
						new_plugin.interval = entry["interval"].get<uint16_t>();
						m_Plugins.push_back(new_plugin);
					}
				}
			}
		}
		if (data.contains("QRZ")) {
			try {
				if (data["QRZ"].is_string()) {
					m_QRZ.Secret = data["QRZ"];
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
	//			debug_log << "CONFIG: Invalid QRZ Passowrd\n";
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
	m_CallSign	= "N0CALL";
	m_PSKCall	= "";
	m_DXMsg.clear();
	m_sats.clear();
	m_rss.clear();
	m_Plugins.clear();
	m_plugin_path	= ".";
	m_cache_path	= ".";
	m_asset_path	= "images";
	m_DE		= {0, 0};
	m_DX		= {0, 0};
	m_QRZ.Secret.clear();
	m_QRZ.Key.clear();
	m_dxserver.name	= "dxfun.com";
	m_dxserver.port	= 8000;
//    load_config();
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

const std::string& config::PluginPath() const {
	return m_plugin_path;
}

const std::string& config::AssetPath() const {
	return m_asset_path;
}

const std::string& config::CachePath() const {
	return m_cache_path;
}

const std::string& config::SiteCache() const {
	return m_site_cache;
}

const std::string& config::PSKCall() const {
	return m_PSKCall;
}

const GeoCoord& config::DE() const {
	return m_DE;
}

const GeoCoord& config::DX() const {
	const std::lock_guard<std::mutex>dx_lock(dx_set_mutex);
	return m_DX;
}

void config::set_DX(const GeoCoord& target, const std::string msg) {
	const std::lock_guard<std::mutex>dx_lock(dx_set_mutex);
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
	const std::lock_guard<std::mutex>dx_lock(dx_set_mutex);
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
	zorder = 0;
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
	zorder = 0;
	return;
}

ScreenFrame* map_overlay::get_overlay(SDL_Renderer* renderer, uint16_t owner, SDL_FRect dims, uint8_t z_layer = 1) {
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
		if (z_layer > 2) {
			debug_log << "OVERLAY: Clamping invalid Z order request.";
			z_layer = 1;
		}
		new_overlay.z_order = z_layer;
		new_overlay.panel.Create(renderer, dims);
		if (!new_overlay.panel.texture) {
			SDL_Log("Failed to create overlay texture: %s", SDL_GetError());
			debug_log << "OVERLAY: Failed to create overlay texture: " << SDL_GetError() << "\n";
			return nullptr;
		} else {
			debug_log << "OVERLAY: Created new overlay texture for module "<< owner << "\n";
		}
		//  SDL_Log ("Created overlay ... %p\t Tex: %p", (void*)&(new_overlay.panel), (void*)(new_overlay.panel.texture));
		debug_log << "OVERLAY: Created "<< dims.w << "x" << dims.h << " " 
			<< ((dims.w*dims.h*4.0)/1024.0) << "KB overlay ... "<< (void*)&(new_overlay.panel) 
			<<"\t Tex: " << (void*)(new_overlay.panel.texture)<< "\n";
		new_overlay.panel.Clear(SDL_Color{0,0,0,255});
		SDL_SetTextureBlendMode(new_overlay.panel.texture, SDL_BLENDMODE_BLEND);
		overlay_list.push_back(std::move(new_overlay));
		return (&(overlay_list.back().panel));
	}
	return nullptr;
}

bool map_overlay::overlay_check(uint16_t owner) {
	for (auto& overlay : overlay_list) {
		if (overlay.owner == owner) {
			return true;
		}
	}
	return false;
}

void map_overlay::set_zorder(Uint8 priority) {
	zorder = priority;
}

ScreenFrame* map_overlay::next_overlay(uint8_t z_layer) {
	if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
		SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
	}
	else {
		SDL_Log("Overlay call during resize event!");
		return (nullptr);
	}
	while (index < overlay_list.size() && overlay_list[index].z_order != z_layer) {
		index++;
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

void map_overlay::remove_overlay(uint16_t owner) {
	for (auto it = overlay_list.begin(); it != overlay_list.end(); ++it) {
		if (it->owner == owner) {
			debug_log << "OVERLAY: Removing overlay for " << owner << "\n";
			it->panel.Reset();
			it = overlay_list.erase(it);
			return;
		}
	}
	return;
}

map_icons::map_icons () {
	clear_icons();
}

map_icons::~map_icons() {
	clear_icons();
}

void map_icons::clear_icons() {
	debug_log << "ICONS: Clearing all icon textures\n";
	if (!icon_list.empty()) {
		for (struct icon_entry& tex : icon_list) {
			if (tex.icon) {
				SDL_DestroyTexture(tex.icon);
				tex.icon = nullptr;
			}
		}
	}
	icon_list.clear();
}

bool map_icons::icon_check(uint16_t index, uint16_t owner) {
	if (icon_list.empty() || index >= icon_list.size()) {
		return false;
	}
	if (icon_list[index].owner == owner) {
		return true;
	} else {
		return false;
	}
}

SDL_Texture* map_icons::get_icon(uint16_t index) {
	if (icon_list.empty() || index > icon_list.size() || index ==0) {
		return nullptr;
	} else {
		return icon_list[index-1].icon;
	}
}

void map_icons::icon_delete(uint16_t owner, uint16_t index) {
	if (icon_list.empty() || index >= icon_list.size()) {
		return;
	}
	if (icon_list[index].owner == owner) {
		SDL_DestroyTexture(icon_list[index].icon);
		icon_list[index].icon = nullptr;
		icon_list[index].owner = 0;
		return;
	} else {
		return;
	}
}

uint16_t map_icons::icon_create(uint16_t owner, SDL_Surface* icon_image) {
	struct icon_entry new_icon;
	new_icon.owner = owner;
	int w, h;
	SDL_GetRenderOutputSize(clock_renderer, &w, &h);
	w =(w/50);
	uint16_t result = 0;
	SDL_Surface* scaled_surface = SDL_CreateSurface(w, w, SDL_PIXELFORMAT_RGBA8888);
	if (scaled_surface) {
		SDL_ClearSurface(scaled_surface, 0,0,0,0);
		if (SDL_BlitSurfaceScaled(icon_image, NULL, scaled_surface, NULL, SDL_SCALEMODE_NEAREST)) {
//			new_icon.icon = SDL_CreateTextureFromSurface(clock_renderer, scaled_surface);
			new_icon.icon = SDL_CreateTexture(clock_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, scaled_surface->w, scaled_surface->h);
			if (new_icon.icon) {
				result = SDL_UpdateTexture(new_icon.icon, NULL, scaled_surface->pixels , scaled_surface->pitch);
				icon_list.push_back(new_icon);
				result = (static_cast<uint16_t>(icon_list.size()));
				debug_log << "ICON: Created icon ID: "<< result << "\n";
			} else {
				debug_log << "ICON: Unable to create texture\n";

			}
		} else {
			debug_log << "ICON: Unable to blit surface\n";
		}
		SDL_DestroySurface(scaled_surface);
	} else {
		debug_log << "ICON: Unable to create scaled surface\n";
	}
	debug_log << "ICON: Returning icon ID: "<< result << "\n";
	return result;
}

bool map_icons::icon_update(uint16_t owner, uint16_t index, SDL_Surface* icon_image) {
	if (icon_list.empty() || index >= icon_list.size()) {
		return false;
	}
	if (icon_list[index].owner != owner) {
		return false;
	}
	if (icon_list[index].icon == nullptr) {
		return false;
	}
	int w, h;
	SDL_GetRenderOutputSize(clock_renderer, &w, &h);
	w =(w/50);
	bool result = false;
	SDL_Surface* scaled_surface = SDL_CreateSurface(w, w, SDL_PIXELFORMAT_RGBA8888);
	if (scaled_surface) {
		SDL_ClearSurface(scaled_surface, 0,0,0,0);
		result = SDL_BlitSurfaceScaled(icon_image, NULL, scaled_surface, NULL, SDL_SCALEMODE_NEAREST);
		if (result) {
			result = SDL_UpdateTexture(icon_list[index].icon, NULL, scaled_surface->pixels , scaled_surface->pitch);
		}
		SDL_DestroySurface(scaled_surface);
	} else {
		result = false;
	}
	return result;
}

