#ifndef ALPORT_MP3_H
#define ALPORT_MP3_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void MP3;

MP3 *mp3_load(const char *filename);
MP3 *mp3_create(void *data, size_t data_len);
void mp3_destroy(MP3 *mp3, int free_buf);
int stream_play_mp3(MP3 *mp3, int loop);
int mp3_get_samplerate(MP3 *mp3);
int mp3_get_channels(MP3 *mp3);

#ifdef __cplusplus
}
#endif

#endif  /* ifndef ALPORT_MP3_H */
