#include "sun.h"
#include <cstddef>
#include <cstdio>
#include <jpeglib.h>
#include <jerror.h>
#include "../utils.h"


void sun_module(ScreenFrame& panel) {

    Uint8* sun_raw = 0 ;
    Uint32 data_size;
    time_t cache_time;
    bool reload_flag = false;
    data_size = cache_loader(MOD_SOLAR, (void**)&sun_raw, &cache_time);
    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
    }

    if (reload_flag) {
        data_size = http_loader("https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_HMIIC.jpg", (void**)&sun_raw);   // live
        if (data_size) {
            add_data_cache(MOD_SOLAR, data_size, sun_raw);
        }
    }

    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;
    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);
    jpeg_stdio_src(&info, sun_raw);
    jpeg_read_header(&info, TRUE);
    jpeg_start_decompress(&info);
    unsigned long int imgWidth, imgHeight;
    int numComponents;
    imgWidth = info.output_width;
	imgHeight = info.output_height;
	numComponents = info.num_components;

	dwBufferBytes = imgWidth * imgHeight * 3; /* We only read RGB, not A */
	lpData = (unsigned char*)malloc(sizeof(unsigned char)*dwBufferBytes);

	lpNewImage = (struct imgRawImage*)malloc(sizeof(struct imgRawImage));
	lpNewImage->numComponents = numComponents;
	lpNewImage->width = imgWidth;
	lpNewImage->height = imgHeight;
	lpNewImage->lpData = lpData;

	/* Read scanline by scanline */
	while(info.output_scanline < info.output_height) {
		lpRowBuffer[0] = (unsigned char *)(&lpData[3*info.output_width*info.output_scanline]);
		jpeg_read_scanlines(&info, lpRowBuffer, 1);
	}

	jpeg_finish_decompress(&info);
	jpeg_destroy_decompress(&info);


	return;
}