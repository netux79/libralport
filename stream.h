#ifndef ALPORT_STREAM_H
#define ALPORT_STREAM_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

enum STREAM_TYPE { STREAM_GME, STREAM_MP3, STREAM_VORBIS, STREAM_NONE = 255 };

int stream_init(float delta);
void stream_deinit(void);
void stream_fill_buffer(void);
void stream_stop(void);
void stream_pause(void);
void stream_resume(void);
int stream_isplaying(void);
int stream_get_samplerate(void);
int stream_get_channels(void);
int stream_get_volume(void);
void stream_set_volume(int volume);
int stream_get_type(void);

#ifdef __cplusplus
}
#endif

#endif  /* ifndef ALPORT_STREAM_H */
