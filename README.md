This is my attempt at re-implementing the idea behind HamClock from Clear Sky Institute as an SDL application eventually with versions for Windows, Mac, and Linux from the same code base.
  The call sign is currently hard coded in this very early version, until I get far enough to implement a config file.
  
<HR>

Library dependancies:
<UL>
 <LI>nlohmann json -- https://github.com/nlohmann/json   (included)</LI>
 <LI>SDL3          -- https://wiki.libsdl.org/SDL3/FrontPage</LI>
 <LI>SDL3-TTF      -- https://wiki.libsdl.org/SDL3_ttf/FrontPage</LI>
 <LI>GNU LibMath   -- https://www.gnu.org/software/libc/</LI>
 <LI>LibCurl      -- https://curl.se/libcurl/</LI>
 <LI>LibSGP4      -- https://github.com/dnwrnr/sgp4</LI>
 </UL>
<HR>

 Data Sources:
 <UL>
 <LI>Nasa Blue Marble      --  https://svs.gsfc.nasa.gov/2915</LI>
 <LI>Nasa Earth at Night   --  https://www.visibleearth.nasa.gov/images/144898/earth-at-night-black-marble-2016-color-maps</LI>
 <LI>Country Border        --  Made with Natural Earth. Free vector and raster map data @ naturalearthdata.com.</LI>
 <LI>POTA Spots            --  https://api.pota.app/spot/activator</LI>
 <LI>Satellite Data        --  https://celestrak.org/NORAD/elements/</LI>
 </UL>
