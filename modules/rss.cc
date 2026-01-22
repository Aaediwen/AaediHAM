#include "rss.h"
#include "../utils.h"
#include <algorithm>
#include <iostream>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

std::vector<rss_feed> rss_feeds;

rss_feed::rss_feed(std::string new_url) {
     m_url.clear();
     m_url = new_url;
     m_entries.clear();
     this->m_rss_lock = SDL_CreateMutex();
     if (!this->m_rss_lock) {
          SDL_Log("Failed to create RSS mutex: %s", SDL_GetError());
     }
     m_current_index = 0;
     fetch_state = 0;
}

rss_feed::~rss_feed() {
     m_url.clear();
     m_entries.clear();
     if (this->m_rss_lock) {
          SDL_DestroyMutex(this->m_rss_lock);
     }
}

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
     htmlDocPtr htmldoc = htmlReadMemory(raw_html.c_str(), raw_html.size(), NULL, "UTF-8", 0);
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
                     }	// inside item

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
          debug_log << "RSS: Calling XML ReadMemory\n";
          xml_tree = xmlReadMemory(raw_xml, data_size, nullptr, nullptr, 0);
          if (!xml_tree) {
               debug_log << "RSS: Failed to parse RSS Feed XML\n";
          } else {
               SDL_LockMutex(this->m_rss_lock);
               m_entries.clear();
               parse_rss(xmlDocGetRootElement(xml_tree), parser_state::none);
               m_current_index = 0;
               fetch_state = 0;
               xmlFreeDoc (xml_tree);
               xml_tree = nullptr;
               SDL_UnlockMutex(this->m_rss_lock);
          }
     } else {
          debug_log << "RSS: Skipped Parsing bad RSS feed\n";
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
          }
     }
     return result;
}


size_t feed_index = 0;
SDL_Surface* ticker_surface = nullptr;
//SDL_Texture* active_ticker_texture = nullptr;
SDL_FRect source_rect, dest_rect;
//, max_rect;
SDL_Rect ticker_texture_size = {0, 0, 0, 0};
SDL_Texture* streaming_ticker = nullptr;
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
    SDL_Rect int_ticker_box;
    mapsize.w = panel.dims.w;
    mapsize.h = panel.dims.h;
    ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), MOD_RSS, mapsize);

    overlay->Clear(SDL_Color{0,0,0,0});
    ticker_box.x = 0;
    ticker_box.y = (mapsize.h/16)*15;
    ticker_box.w = mapsize.w;
    ticker_box.h = (mapsize.h/16);
    dest_rect.h = ticker_box.h;
    dest_rect.y = ticker_box.y;
    int_ticker_box.x=static_cast<int>(ticker_box.x);
    int_ticker_box.y=static_cast<int>(ticker_box.y);
    int_ticker_box.h=static_cast<int>(ticker_box.h);
    int_ticker_box.w=static_cast<int>(ticker_box.w);
    if ((ticker_texture_size.h != int_ticker_box.h) &&(ticker_texture_size.w != int_ticker_box.w)) {
        if (streaming_ticker) {
            SDL_DestroyTexture(streaming_ticker);
            streaming_ticker=nullptr;
        }
        streaming_ticker = SDL_CreateTexture(panel.GetRenderer(), overlay->texture->format, SDL_TEXTUREACCESS_STREAMING, int_ticker_box.w, int_ticker_box.h);
        if (streaming_ticker) {
            ticker_texture_size = int_ticker_box;
        }
    }
    // get the next headline as needed
    if (!ticker_surface) {
        std::cout << "RSS: Getting next headline\n";
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
//             SDL_Log (next_text.c_str());
//             std::cout << "RSS: Rendering Headline text to surface\n";
             ticker_surface = TTF_RenderText_Shaded(Sans, next_text.c_str(), next_text.size(), SDL_Color{128,64,64,255}, SDL_Color{0,0,0,0});

             if (ticker_surface) {
                   // init source and dest boxes
//                   SDL_SetSurfaceColorKey(ticker_surface, 1, 0);
                   source_rect.h = ticker_surface->h;
                   source_rect.w = 0;
                   source_rect.x = 0;
                   source_rect.y = 0;
                   dest_rect.h = ticker_box.h;
                   dest_rect.w = 0;
//                   dest_rect.x = ticker_surface->w;
                   dest_rect.x = ticker_box.w;
                   dest_rect.y = 0;
             }  else {
                        debug_log << "RSS: Unable to create Ticker Surface: "<< SDL_GetError()<< "\n";
                   }

        }
    }

    // bail if there isn't an avtive headline at this point
    if (!ticker_surface) {
         std::cout << "RSS: No Ticker text to render\n";
         return;
    }

    // draw the background
    SDL_SetRenderTarget(panel.GetRenderer(), overlay->texture);
    SDL_SetRenderDrawColor (panel.GetRenderer(), 255,128,128,128);
    SDL_RenderFillRect(panel.GetRenderer(), &ticker_box);

    if (ticker_surface) {
         // we have an active headline here
//         std::cout << "RSS: Attempting to render headline to overlay\n";
         SDL_Surface* texture_surface;
         SDL_Rect int_source_box;
         int_source_box.x = static_cast<int>(source_rect.x);
         int_source_box.y = static_cast<int>(source_rect.y);
         int_source_box.h = static_cast<int>(source_rect.h);
         int_source_box.w = static_cast<int>(source_rect.w);
//         SDL_Rect int_dest_box;
//         int_dest_box.x = static_cast<int>(dest_rect.x);
//         int_dest_box.y = static_cast<int>(dest_rect.y);
//         int_dest_box.h = static_cast<int>(dest_rect.h);
//         int_dest_box.w = static_cast<int>(dest_rect.w);

         if (streaming_ticker) {
//              std::cout << "RSS: Locking Streaming Texture\n";
              if (SDL_LockTextureToSurface(streaming_ticker, NULL, &texture_surface)) {
                   SDL_ClearSurface(texture_surface, 0, 0, 0, 0);
                   if (SDL_BlitSurfaceScaled(ticker_surface, &int_source_box, texture_surface, NULL, SDL_SCALEMODE_NEAREST)) {
//                      std::cout << "RSS: Blitted Texture " << int_source_box.x << "," <<int_source_box.w << " " <<int_dest_box.x << ", " << int_dest_box.w << "\n";
                   }
                   SDL_UnlockTexture(streaming_ticker);
//                   std::cout << "RSS: UnLocked Streaming Texture --- rendering\n";
                   if (SDL_RenderTexture(panel.GetRenderer(), streaming_ticker, NULL, &dest_rect)) {
//                        std::cout << "RSS: Rendered to Overlay Surface: " << SDL_GetError() << "\n";
                   } else {
                       std::cout << "RSS: Unable to render to Overlay Surface: " << SDL_GetError() << "\n";
                   }
              } else {
                   std::cout << "RSS: Unable to lock Overlay Surface: " << SDL_GetError() << "\n";
              }
         }
         // update the scroller
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
         // calculate dest string width
         dest_rect.w = ticker_box.w - dest_rect.x ;
         if (dest_rect.w > source_rect.w) {
             dest_rect.w = source_rect.w;
         } else {
             source_rect.w = dest_rect.w;
         }
         // nuke the headline to load the next
         if ((dest_rect.x == 0) && (source_rect.w ==0)) {
              SDL_DestroySurface(ticker_surface);
              ticker_surface = nullptr;
         }


    } else {
        std::cout << "Scroller seems to be missing the ticker_surface\n";
    }
     return;
}