#include "camera_logic.h"

void camera_logic_pick_frame(bool psram_present,
                             framesize_t *frame_size,
                             int *jpeg_quality,
                             camera_fb_location_t *fb_location)
{
    *frame_size = psram_present ? FRAMESIZE_SVGA : FRAMESIZE_VGA;
    *jpeg_quality = psram_present ? 12 : 14;
    *fb_location = psram_present ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
}
