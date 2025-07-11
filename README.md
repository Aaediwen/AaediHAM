This is my attempt at re-implementing the idea behind HamClock from Clear Sky Institute as an SDL application eventually with versions for Windows, Mac, and Linux from the same code base.
  
The program now requires a config file (aaediclock_config.json) in the Current Directory. 

<HR>

Config File Format:

<CODE>
{
     "CallSign": "N0CALL",             // your callsign here
     "DE": {                           // your location
          "Latitude": 37.978,         
          "Longitude": -84.495
     },
     "DX": {                          // the location you want in the DX box
          "Latitude": 0.0,
          "Longitude": 0.0
     },
     "QRZ": [                        // QRZ password, added by command line argument.
          10,                       // if you want to type yours in here, use:
          10,                        // "QRZ": "QRZ_PASSWORD"
          10,                       // this will work, instead, but it will mean your password is in cleartext
          10,                        // better to use --QRZ_Pass command line option to set this
          10,
          10,
          10,
          10,
          10
     ],
     "SatList": [                    // a list of satellites to track from 
          "LES-5",                  // "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=tle"
          "NOAA 15",
          "ISS (ZARYA)",
          "COSMOS 2409",
          "NOAA 18"
     ]
}
</CODE>

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
 <LI>DX Spots              --  Telnet: dxfun.com:8000 </LI> 
 <LI>QRZ API               --  http://xmldata.qrz.com/</LI>  
 </UL>
