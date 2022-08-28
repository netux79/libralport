#ifndef ALPORT_VORBIS_H
#define ALPORT_VORBIS_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void VORBIS;

VORBIS *vorbis_load(const char *filename);
VORBIS *vorbis_create(void *data, size_t data_len);
void vorbis_destroy(VORBIS *vorbis, int free_buf);
int stream_play_vorbis(VORBIS *vorbis, int loop);
int vorbis_get_samplerate(VORBIS *vorbis);
int vorbis_get_channels(VORBIS *vorbis);

#ifdef __cplusplus
}
#endif

#endif  /* ifndef ALPORT_VORBIS_H */
