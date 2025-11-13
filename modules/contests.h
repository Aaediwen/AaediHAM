#include "../aaediclock.h"

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

void contest_module(ScreenFrame& panel);
