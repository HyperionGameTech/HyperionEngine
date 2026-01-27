#ifndef HYP_PHYSICAL_CAMERA
#define HYP_PHYSICAL_CAMERA

/* Physical camera */
static const float aperture = 16.0;
static const float shutter = 1.0/125.0;
static const float sensitivity = 100.0;
static const float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
static const float exposure = 1.0 / (1.2 * pow(2.0, ev100));

#endif