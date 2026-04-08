#ifndef CONTEST_H
#define CONTEST_H
#include "aaediclock.h"
#include "plugin_api.h"

// https://www.contestcalendar.com/calendar.rss


struct contest {
    std::string title;
    std::string link;
    std::string description;
    std::string guid;
};



/*
<item>
<title>Worldwide Sideband Activity Contest</title>
<link>https://www.contestcalendar.com/weeklycontdetails.php?ref=003650wg</link>
<description>0100Z-0159Z, Nov 11</description>
<guid>https://www.contestcalendar.com/?g=00t5jes0019171</guid>
</item>
*/





class DllExport contest_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif