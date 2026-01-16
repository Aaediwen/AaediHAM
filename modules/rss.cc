#include "rss.h"
#include "../utils.h"
#include <algorithm>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

std::vector<rss_feed> rss_feeds;
std::string spaces;

rss_feed::rss_feed(std::string new_url) {
    m_url.clear();
    m_url = new_url;
    m_entries.clear();
    this->m_rss_lock = SDL_CreateMutex();
    if (!this->m_rss_lock) {
        SDL_Log("Failed to create RSS mutex: %s", SDL_GetError());
    }
    m_current_index = 0;
    fetch_state = 1;
}

rss_feed::~rss_feed() {
    m_url.clear();
    m_entries.clear();
    if (this->m_rss_lock) {
        SDL_DestroyMutex(this->m_rss_lock);
    }
}
/*
ScreenFrame::ScreenFrame(ScreenFrame&& source) noexcept {       // move to new instance
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

ScreenFrame& ScreenFrame::operator=(ScreenFrame&& source) noexcept {    // move over existing
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


*/
rss_feed::rss_feed(rss_feed&& source) noexcept {			// move new

          if (m_rss_lock) {
               SDL_DestroyMutex(m_rss_lock);
          }
          m_rss_lock = source.m_rss_lock;
          source.m_rss_lock = nullptr;
          m_url = std::move(source.m_url);
          m_title = std::move(source.m_title);
          m_entries = std::move(source.m_entries);
          fetch_state = std::move(source.fetch_state);
          m_current_index = std::move(source.m_current_index);
     return;
}

rss_feed& rss_feed::operator=(rss_feed&& source) noexcept {		// move existing
     if (this != &source) {
          if (m_rss_lock) {
               SDL_DestroyMutex(m_rss_lock);
          }
          m_rss_lock = source.m_rss_lock;
          source.m_rss_lock = nullptr;
          m_url = std::move(source.m_url);
          m_title = std::move(source.m_title);
          m_entries = std::move(source.m_entries);
          fetch_state = std::move(source.fetch_state);
          m_current_index = std::move(source.m_current_index);
     }
     return *this;
}

rss_feed::rss_feed(const rss_feed& source) {				// copy new
      if (!m_rss_lock) {
           m_rss_lock = SDL_CreateMutex();
      }
      m_url = source.m_url;
      m_title = source.m_title;
      m_entries = source.m_entries;
      fetch_state = source.fetch_state;
      m_current_index = source.m_current_index;
      return;
}

rss_feed& rss_feed::operator=(const rss_feed& source) {			// copy existing
      if (this != &source) {
           if (!m_rss_lock) {
               m_rss_lock = SDL_CreateMutex();
           }
           m_url = source.m_url;
           m_title = source.m_title;
           m_entries = source.m_entries;
           fetch_state = source.fetch_state;
           m_current_index = source.m_current_index;
      }
      return *this;
}


std::string rss_feed::strip_html(xmlNode* start_node) {
     xmlNode* current_node = nullptr;
     std::string result;
     result.clear();
     for (current_node = start_node; current_node; current_node = current_node->next) {
          if (!current_node->children) {
              result += reinterpret_cast<const char*>(xmlNodeGetContent(current_node));
          } else {
               result += strip_html(current_node->children);
          }
     }

     return result;
}


std::string xml_raw_text(xmlNode* parent) {
     xmlNode* current_node;
     std::string raw_text;
     raw_text.clear();
     if (parent) {
         if ((parent->type == XML_TEXT_NODE) ) {
//         if ((parent->type == XML_TEXT_NODE) || (parent->type == XML_CDATA_SECTION_NODE)) {
             SDL_Log("RSS: RAW TEXT");
             raw_text = reinterpret_cast<const char*>(parent->content);
         } else if ((parent->type == XML_CDATA_SECTION_NODE)) {
             SDL_Log("RSS: CDATA");
             raw_text = reinterpret_cast<const char*>(parent->content);
         } else {
             if (parent->children) {
                  for (current_node = parent->children; current_node; current_node = current_node->next) {
                      raw_text += xml_raw_text(current_node);
                  }
             }
         }
     }
     return raw_text;
}

void rss_feed::parse_rss(xmlNode* start_node, enum parser_state parent) {
     xmlNode* current_node = nullptr;
     for (current_node = start_node; current_node; current_node = current_node->next) {
           if (current_node->type == XML_ELEMENT_NODE) {
                std::string NodeName(reinterpret_cast<const char*>(current_node->name));
                std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
                if ((NodeName == "rss")||(NodeName == "channel")) {
                     spaces.push_back('-');
                     parse_rss(current_node->children, parser_state::channel);
                     if (!spaces.empty()) {
                          spaces.pop_back();
                     }
                } else if (NodeName == "item") {
                     spaces.push_back('*');
                     parse_rss(current_node->children, parser_state::item);

                     if (!spaces.empty()) {
                          spaces.pop_back();
                     }
                } else if (NodeName == "description") {
                     if (parent == parser_state::item) {
                          std::string content = xml_raw_text(current_node);

                          xmlDoc * html_tree = htmlReadMemory(content.c_str(), content.size(), "UTF-8", NULL, HTML_PARSE_NOBLANKS );
                          if (html_tree) {
                             xmlNode* html_current_node;
                             content.clear();
                             for (html_current_node = html_tree->children; html_current_node; html_current_node = html_current_node->next) {
                                 content += xml_raw_text(html_current_node);
                             }
                             free(html_tree);
                          }
                          m_entries.push_back(content);
                     }

                } else if (NodeName == "title") {
                      if (parent == parser_state::channel) {
                           std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                           m_title = xml_content;


                      }
                }
           }
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
//          SDL_Log("RSS: Calling ReadMemory");
          xml_tree = xmlReadMemory(raw_xml, data_size, "rss.xml", NULL, 0);
          if (!xml_tree) {
               debug_log << "RSS: Failed to parse RSS Feed XML\n";
          } else {
                 SDL_LockMutex(this->m_rss_lock);
                 m_entries.clear();
//                 SDL_Log("RSS: Parsing RSS feed for: %s", m_url.c_str());
                 parse_rss(xmlDocGetRootElement(xml_tree), parser_state::none);
//                 SDL_Log("RSS: Parsing RSS feed done");
                 m_current_index = 0;
                 fetch_state = 0;
                 SDL_UnlockMutex(this->m_rss_lock);
          }
          free (raw_xml);
          raw_xml=nullptr;
     } else {
//          SDL_Log("RSS: Skipped Parsing bad RSS feed");
     }
     if (xml_tree) {
          free (xml_tree);
          xml_tree = nullptr;
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
     if (fetch_state > 200) {
        fetch_state = 0;
     }
     if (fetch_state == 0) {
     if (SDL_TryLockMutex(this->m_rss_lock)) {
          if (m_entries.empty() || (m_current_index >= m_entries.size())) {
               thread = SDL_CreateThread(thread_launcher, "RSS Fetcher", this);
               SDL_DetachThread(thread);
               fetch_state = 1;
          } else {
               result = m_title+": "+m_entries[m_current_index];
               m_current_index++;
          }
          SDL_UnlockMutex(this->m_rss_lock);
     } else {
         debug_log << "RSS: Mutex Lock Fail, no fetch attempted: "<< SDL_GetError()<<"\n";
         fetch_state++;
     }
     } else { fetch_state++; }
     return result;
}


/*
SDL_Surface* active_ticker = nullptr;
SDL_Texture* active_ticker_texture = nullptr;
SDL_FRect source_rect;
unsigned long int ticker_index = 0;
int delay_timer = 0;
void rss_ticker(ScreenFrame& panel) {
     if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
         SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
     }
     else {
         SDL_Log("RSS DRAW during resize event!");
         return;
     }
     if (!Sans) {
        debug_log << "RSS: No font defined\n";
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "RSS: Missing Renderer!\n";
        return;
    }
    if (!panel.texture) {
        debug_log << "RSS: Missing PANEL!\n";
        return;
    }
    if (rss_feeds.empty()) {
        if (!clockconfig.Rss().empty()) {
            for (const std::string& stropt : clockconfig.Rss()) {
                 rss_feeds.emplace_back(stropt);
            }
        }
    }
    if (rss_feeds.empty()) {
        return;
    }
    if (!active_ticker) {
//         std::string new_ticker = htmldecode(rss_feeds[ticker_index].next());
         std::string new_ticker = rss_feeds[ticker_index].next();
         ticker_index++;
         if (ticker_index >= rss_feeds.size()) {
              ticker_index = 0;
         }
         if (!new_ticker.empty()) {
              new_ticker.insert(0, 20, ' ');
              new_ticker.append(20, ' ');

              debug_log << "RSS: " << new_ticker << "\n";
              SDL_Log("RSS: New Ticker: %s", new_ticker.c_str());
              active_ticker = TTF_RenderText_Shaded(Sans, new_ticker.c_str(), new_ticker.size(), SDL_Color{255,64,64,255}, SDL_Color{0,0,0,0});
              if (active_ticker) {
                   source_rect.x=0;
                   source_rect.y=0;
                   source_rect.w = active_ticker->w;
                   if (active_ticker->w > panel.dims.w) {
                       source_rect.w = panel.dims.w;
                   }
                   source_rect.h=active_ticker->h;
                   active_ticker_texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), active_ticker);
              }
              delay_timer = 0;

         }
    }
    SDL_FRect mapsize ;
    SDL_FRect ticker_box;
    mapsize.w = panel.dims.w;
    mapsize.h = panel.dims.h;
    ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), MOD_RSS, mapsize);
    overlay->Clear(SDL_Color{0,0,0,0});
    if (!active_ticker) {
         return;
    }
    ticker_box.x = 0;
    ticker_box.y = (mapsize.h/16)*15;
    ticker_box.w=mapsize.w;
    ticker_box.h=(mapsize.h/16);

    SDL_SetRenderTarget(panel.GetRenderer(), overlay->texture);
    SDL_SetRenderDrawColor (panel.GetRenderer(), 255,128,128,128);
    SDL_RenderFillRect(panel.GetRenderer(), &ticker_box);
    SDL_RenderTexture(panel.GetRenderer(), active_ticker_texture, &source_rect, &ticker_box);
    if (source_rect.x + source_rect.w >= active_ticker->w) {
        delay_timer++;
        if (delay_timer > 200) {
           source_rect.x=0;
           ticker_box.w = mapsize.w;
           SDL_DestroyTexture(active_ticker_texture);
           SDL_DestroySurface(active_ticker);
           active_ticker = nullptr;

        }
    } else if (source_rect.x < active_ticker->w) {
        source_rect.x +=2;
    }
    return;
}
*/
size_t feed_index = 0;
SDL_Surface* ticker_surface = nullptr;
SDL_Texture* active_ticker_texture = nullptr;
SDL_FRect source_rect, dest_rect;
const float feed_rate = 2.0f;
void rss_ticker(ScreenFrame& panel) {
     // input validation
     if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
         SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
     }
     else {
         SDL_Log("RSS DRAW during resize event!");
         return;
     }
     if (!Sans) {
        debug_log << "RSS: No font defined\n";
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "RSS: Missing Renderer!\n";
        return;
    }
    if (!panel.texture) {
        debug_log << "RSS: Missing PANEL!\n";
        return;
    }
    // populate the feeds from config if needed
    if (rss_feeds.empty()) {
        if (!clockconfig.Rss().empty()) {
            for (const std::string& stropt : clockconfig.Rss()) {
                 rss_feeds.emplace_back(stropt);
            }
        }
    }
    // go ahead and bail if none are configured
    if (rss_feeds.empty()) {
        return;
    }
    // Init and clear the overlay
    SDL_FRect mapsize ;
    SDL_FRect ticker_box;
    mapsize.w = panel.dims.w;
    mapsize.h = panel.dims.h;
    ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), MOD_RSS, mapsize);
    overlay->Clear(SDL_Color{0,0,0,0});
    ticker_box.x = 0;
    ticker_box.y = (mapsize.h/16)*15;
    ticker_box.w=mapsize.w;
    ticker_box.h=(mapsize.h/16);
    dest_rect.h = ticker_box.h;
    dest_rect.y = ticker_box.y;

    // get the next headline as needed
    if (!active_ticker_texture) {
        std::string next_text;
        // get the next ticker headline
        if (!rss_feeds.empty()) {
             next_text = rss_feeds[feed_index].next();
             feed_index++;
             if (feed_index >= rss_feeds.size()) {
                  feed_index = 0;
             }
        }
        // build the text texture
        if (!next_text.empty()) {
             // surface first
             SDL_Log (next_text.c_str());
             ticker_surface = TTF_RenderText_Shaded(Sans, next_text.c_str(), next_text.size(), SDL_Color{255,64,64,255}, SDL_Color{0,0,0,0});
             if (ticker_surface) {
                   active_ticker_texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), ticker_surface);
                   if (active_ticker_texture) {
                        // init source and dest boxes
                        source_rect.h = ticker_surface->h;
                        source_rect.w = 0;
                        source_rect.x = 0;
                        source_rect.y = 0;
                        dest_rect.h = ticker_box.h;
                        dest_rect.w = 0;
                        dest_rect.x = ticker_box.w;
                        dest_rect.y = ticker_box.y;

                   }
             }
        }
    }

    // bail if there isn't an avtive headline at this point
    if (!active_ticker_texture) {
         return;
    }

    // draw the background
    SDL_SetRenderTarget(panel.GetRenderer(), overlay->texture);
    SDL_SetRenderDrawColor (panel.GetRenderer(), 255,128,128,128);
    SDL_RenderFillRect(panel.GetRenderer(), &ticker_box);

    if (active_ticker_texture) {
         // we have an active headline here
         SDL_RenderTexture(panel.GetRenderer(), active_ticker_texture, &source_rect, &dest_rect);
         // scroll output left
         if (dest_rect.x >0) {
             dest_rect.x -= feed_rate;
             if (dest_rect.x < 0) { dest_rect.x = 0; }
         }
         // if output is at the far left, then scroll input start right
         if (dest_rect.x == 0 && (source_rect.x < ticker_surface->w)) {
              source_rect.x += feed_rate;
              if (source_rect.x > ticker_surface->w) {
                   source_rect.x = ticker_surface->w;
              }
         }
         // calculate source string width
         source_rect.w = ticker_surface->w - source_rect.x;
//         if (source_rect.w > dest_rect.w) {
//              source_rect.w = dest_rect.w;
//         }
         // calculate dest string width
         dest_rect.w = ticker_box.w - dest_rect.x ;
//         if (dest_rect.w > source_rect.w) {
//             dest_rect.w = source_rect.w;
//         }
         if (dest_rect.w > source_rect.w) {
             dest_rect.w = source_rect.w;
         } else {
             source_rect.w = dest_rect.w;
         }
         // nuke the headline to load the next
         if ((dest_rect.x == 0) && (source_rect.w ==0)) {
              SDL_DestroyTexture(active_ticker_texture);
              SDL_DestroySurface(ticker_surface);
              active_ticker_texture = nullptr;
              ticker_surface = nullptr;
         }


    }
     return;
}