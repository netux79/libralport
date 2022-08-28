#ifndef ALPORT_GME_H
#define ALPORT_GME_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void GME;
enum GME_TYPE { GME_NSF, GME_GBS, GME_SPC, GME_NONE = 255 };


GME *gme_load(const char *filename);
GME *gme_create(void *buf, size_t size, GME_TYPE type);
void gme_destroy(GME *gme);
int gme_track_count(GME *gme);
void gme_change_track(int track);
int stream_play_gme(GME *gme);
int gme_get_samplerate(GME *gme);
int gme_get_channels(GME *gme);

#ifdef __cplusplus
}
#endif

#endif          /* ifndef ALPORT_GME_H */
