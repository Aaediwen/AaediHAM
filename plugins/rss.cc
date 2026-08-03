#include "rss.h"
#include "utils/http_fetch.h"
#include <algorithm>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

// RSS Class definitions


std::vector<rss_feed> rss_feeds;
aaediclock_host_api* host_api = nullptr;

rss_feed::rss_feed(std::string new_url) {
     m_url.clear();
     m_url = new_url;
     m_entries.clear();
//     this->m_rss_lock = SDL_CreateMutex();
//     if (!this->m_rss_lock) {
//          SDL_Log("Failed to create RSS mutex: %s", SDL_GetError());
//     }
     m_current_index = 0;
     fetch_state = 0;
}

rss_feed::~rss_feed() {
     m_url.clear();
     m_entries.clear();
//     if (this->m_rss_lock) {
//          SDL_DestroyMutex(this->m_rss_lock);
//     }
}

rss_feed::rss_feed(rss_feed&& source) noexcept {                        // move new
//     if (m_rss_lock) {
//          SDL_DestroyMutex(m_rss_lock);
//     }
//     m_rss_lock = source.m_rss_lock;
//     source.m_rss_lock = nullptr;
     m_url = std::move(source.m_url);
     m_title = std::move(source.m_title);
     m_entries = std::move(source.m_entries);
     fetch_state = std::move(source.fetch_state);
     m_current_index = std::move(source.m_current_index);
     return;
}

rss_feed& rss_feed::operator=(rss_feed&& source) noexcept {             // move existing
     if (this != &source) {
//          if (m_rss_lock) {
//               SDL_DestroyMutex(m_rss_lock);
//          }
//          m_rss_lock = source.m_rss_lock;
//          source.m_rss_lock = nullptr;
          m_url = std::move(source.m_url);
          m_title = std::move(source.m_title);
          m_entries = std::move(source.m_entries);
          fetch_state = std::move(source.fetch_state);
          m_current_index = std::move(source.m_current_index);
     }
     return *this;
}

rss_feed::rss_feed(const rss_feed& source) {                            // copy new
//     if (!m_rss_lock) {
//          m_rss_lock = SDL_CreateMutex();
//     }
     m_url = source.m_url;
     m_title = source.m_title;
     m_entries = source.m_entries;
     fetch_state = source.fetch_state;
     m_current_index = source.m_current_index;
     return;
}

rss_feed& rss_feed::operator=(const rss_feed& source) {                 // copy existing
     if (this != &source) {
//          if (!m_rss_lock) {
//               m_rss_lock = SDL_CreateMutex();
//          }
          m_url = source.m_url;
          m_title = source.m_title;
          m_entries = source.m_entries;
          fetch_state = source.fetch_state;
          m_current_index = source.m_current_index;
     }
     return *this;
}

std::string rss_feed::strip_html(const std::string& raw_html) {
     // routine to strip HTML tags
          // init
     std::string result;
     result.clear();
     if (raw_html.empty()) {
          return raw_html;
     }
     result = raw_html;
          // attempt to parse HTML
     htmlDocPtr htmldoc = htmlReadMemory(raw_html.c_str(), static_cast<int>(raw_html.size()), NULL, "UTF-8", 0);
     if (htmldoc) {
          // extract the content
          xmlChar* xml_content = xmlNodeGetContent(xmlDocGetRootElement(htmldoc));
          xmlFreeDoc (htmldoc);
          if (xml_content) {
               result = reinterpret_cast<char*>(xml_content);
               xmlFree(xml_content);
          }
     }
     return result;
}

void rss_feed::fetch_description(const xmlNode* source) {
     std::string content;
     content.clear();
     // extract the node content
     xmlChar* raw = xmlNodeGetContent(source);
     if (raw) {
          content = reinterpret_cast<char*>(raw);
          xmlFree(raw);
          // attempt to clean up HTML
          std::string html_stripped = strip_html(content);
          if (!html_stripped.empty()) {
               content = html_stripped;
          }
          // push to the stack
          m_entries.push_back(content);
     }

}

void rss_feed::parse_rss(xmlNode* start_node, enum parser_state parent) {
     xmlNode* current_node = nullptr;
     for (current_node = start_node; current_node; current_node = current_node->next) {
          if (current_node->type == XML_ELEMENT_NODE) {
               std::string NodeName(reinterpret_cast<const char*>(current_node->name));
               std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
                if ((NodeName == "rss")||(NodeName == "channel")) {
                     parse_rss(current_node->children, parser_state::channel);
                } else if (NodeName == "item") {
                     parse_rss(current_node->children, parser_state::item);
                } else if (NodeName == "description") {
                     if (parent == parser_state::item) {
                          fetch_description(current_node);
                     }  // inside item

                } else if (NodeName == "title") {
                     if (parent == parser_state::channel) {
                          std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                          m_title = xml_content;
                     }
                }
          } // XML_ELEMENT_NODE
     }
     return;
}


void rss_feed::fetch_rss() {
     char* raw_xml = 0 ;
     Uint64 data_size = 0;
     xmlDocPtr xml_tree = 0;
     SDL_Log("RSS: Fetching RSS feed for: %s", m_url.c_str());
     data_size = http_loader(m_url.c_str(), (void**)&raw_xml);
     if (data_size > 50) {
          *(host_api->AaediHAM_LogDebug) << "RSS: Calling XML ReadMemory\n";
          xml_tree = xmlReadMemory(raw_xml, static_cast<int>(data_size), nullptr, nullptr, 0);
          if (!xml_tree) {
               *(host_api->AaediHAM_LogDebug) << "RSS: Failed to parse RSS Feed XML\n";
          } else {
               const std::lock_guard<std::mutex>rss_lock(m_rss_lock);
//               SDL_LockMutex(this->m_rss_lock);
               m_entries.clear();
               parse_rss(xmlDocGetRootElement(xml_tree), parser_state::none);
               m_current_index = 0;
               fetch_state = 0;
               xmlFreeDoc (xml_tree);
               xml_tree = nullptr;
//               SDL_UnlockMutex(this->m_rss_lock);
          }
     } else {
          *(host_api->AaediHAM_LogDebug) << "RSS: Skipped Parsing bad RSS feed\n";
     }
     if (raw_xml) {
          free (raw_xml);
          raw_xml = nullptr;
     }
     return;
}

int SDLCALL rss_feed::thread_launcher(void* data) {
(void)data;
    auto* self = (rss_feed*)data;
    self->fetch_rss();
    return 0;
}

std::string rss_feed::next() {
     std::string result;
     result.clear();
     SDL_Thread* thread = nullptr;
     if (fetch_state == 0) {
          if (this->m_rss_lock.try_lock()) {
//          if (SDL_TryLockMutex(this->m_rss_lock)) {
               if (m_entries.empty() || (m_current_index >= m_entries.size())) {
                    thread = SDL_CreateThread(thread_launcher, "RSS Fetcher", this);
                    if (thread) {
                        SDL_DetachThread(thread);
                        fetch_state = 1;
                    } else {
                        fetch_state = 0;
                    }
               } else {
                    result = m_title+": "+m_entries[m_current_index];
                    m_current_index++;
               }
               this->m_rss_lock.unlock();
//               SDL_UnlockMutex(this->m_rss_lock);
          } else {
               *(host_api->AaediHAM_LogDebug) << "RSS: Mutex Lock Fail, no fetch attempted: "<< SDL_GetError()<<"\n";
          }
     }
     return result;
}



// end RSS feed class definitions

// plugin class definitions

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new rss_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void rss_plugin::plugin_init() const {
    // populate the feeds from config if needed
    if (rss_feeds.empty()) {
        std::string feed_url;
        const char* temp = host_api->AaediHAM_ConfigGetNextRss();
        if (temp) {
            feed_url = temp;
        }
        if (!feed_url.empty()) {
            while (!feed_url.empty()) {
                 rss_feeds.emplace_back(feed_url);
                 feed_url.clear();
                 temp = host_api->AaediHAM_ConfigGetNextRss();
                 if (temp) {
                     feed_url = temp;
                 }
            }
        }
    }

    return;
}

void rss_plugin::plugin_exit() const {
    return;
}

size_t feed_index = 0;
bool restart_flag = false;
aaediclock_FRect scroller_size = {0.0, 0.0, 0.0, 0.0};
float scroller_start_pos = 0.0;
SDL_Time last_update_time = 0;
void rss_plugin::plugin_main(const aaediclock_FRect& dims) const {
    // go ahead and bail if none are configured
    if (rss_feeds.empty()) {
        return;
    }

    std::string next_text;
    // get the next ticker headline
    if (!restart_flag) {
    if (!rss_feeds.empty()) {
        next_text = rss_feeds[feed_index].next();
        *(host_api->AaediHAM_LogDebug) << "headline: " << next_text << "\n";
        if (!next_text.empty()) {
            scroller_size = host_api->AaediHAM_ScrollerInit (next_text.c_str(), aaediclock_Color{128, 128, 192, 255}, aaediclock_Color{128,0,0,0});
            restart_flag = true;
            scroller_start_pos = host_api->AaediHAM_GetMapSize().w;
            SDL_GetCurrentTime(&last_update_time);
        }
        feed_index++;
        if (feed_index >= rss_feeds.size()) {
            feed_index = 0;
        }
    }
    }
    aaediclock_FRect mapsize = host_api->AaediHAM_GetMapSize();
    host_api->AaediHAM_OverlaySet(mapsize, OVERLAY_FOREGROUND);
    host_api->AaediHAM_OverlayClear(aaediclock_Color{0,0,0,0});
    aaediclock_FRect ticker_box;
    ticker_box.x = 0;
    ticker_box.y = (mapsize.h/16)*15;
    ticker_box.w = mapsize.w;
    ticker_box.h = (mapsize.h/16);
    host_api->AaediHAM_GraphicsDrawRect (aaediclock_Color{128,0,0,128}, ticker_box, 1);
//    aaediclock_FRect ticker_box;
    ticker_box.x = scroller_start_pos;
    ticker_box.y = (mapsize.h/16)*15;
    ticker_box.w = mapsize.w;
    ticker_box.h = (mapsize.h/16);
    host_api->AaediHAM_ScrollerPosition(scroller_size, ticker_box);
    SDL_Time currenttime;
    SDL_GetCurrentTime(&currenttime);
    SDL_Time time_offset = (currenttime - last_update_time)/100000000;
    scroller_start_pos -= ((mapsize.w/100) * time_offset);
    if (scroller_start_pos < 0.0) {
       scroller_size.x -= scroller_start_pos;
       scroller_start_pos = 0.0;
    }
    last_update_time = currenttime;
    if (scroller_size.x > scroller_size.w) {
        restart_flag = false;
    }

}

const char* rss_plugin::getName() const {
    return "Rss Module";
}

void rss_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

// end plugin definition