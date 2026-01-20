#include "../aaediclock.h"
#include <libxml/parser.h>


class rss_feed {
    public:
        rss_feed(std::string new_url);
        ~rss_feed();
        rss_feed(rss_feed&& source) noexcept;
        rss_feed& operator=(rss_feed&& source) noexcept;
        rss_feed(const rss_feed& source);
        rss_feed& operator=(const rss_feed& source);

        std::string next();
        void refresh();

    private:
        enum class parser_state {
            channel,
            item,
            none
        };
        std::string m_url;
        std::string m_title;
        std::vector<std::string> m_entries;
        SDL_Mutex* m_rss_lock = nullptr;
        int fetch_state = 0;
        unsigned long int m_current_index = 0;
        std::string strip_html(const std::string& raw_html);
        void fetch_description(const xmlNode* source);
        void parse_rss(xmlNode* start_node, enum parser_state parent_name);
        void SDLCALL fetch_rss();
        static int SDLCALL thread_launcher(void* data);


//        void fetch_rss();
};


void rss_ticker(ScreenFrame& panel);
