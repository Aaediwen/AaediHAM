This is my attempt at re-implementing the idea behind HamClock from Clear Sky Institute as an SDL application eventually with versions for Windows, Mac, and Linux from the same code base.
  The call sign is currently hard coded in this very early version, until I get far enough to implement a config file.

Library dependancies:

 nlohmann json -- https://github.com/nlohmann/json
 
 SDL3          -- https://wiki.libsdl.org/SDL3/FrontPage
 
 SDL3-TTF      -- https://wiki.libsdl.org/SDL3_ttf/FrontPage
 
 GNU LibMath   -- https://www.gnu.org/software/libc/

 Data Sources:
 
 Nasa Blue Marble      --  https://svs.gsfc.nasa.gov/2915
 
 Nasa Earth at Night   --  https://www.visibleearth.nasa.gov/images/144898/earth-at-night-black-marble-2016-color-maps
 
 Country Border        --  Made with Natural Earth. Free vector and raster map data @ naturalearthdata.com.
 
 POTA Spots            --  https://api.pota.app/spot/activator
 
