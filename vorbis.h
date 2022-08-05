#ifndef ALPORT_VORBIS_H
#define ALPORT_VORBIS_H

#ifdef __cplusplus
extern "C" {
#endif


typedef void VORBIS;

VORBIS *vorbis_load(const char *filename);
VORBIS *vorbis_create(void *data, size_t data_len);
void vorbis_destroy(VORBIS *vorbis, int free_buf);
int vorbis_init(float delta);
void vorbis_deinit(void);
int vorbis_play(VORBIS *vorbis, int loop);
void vorbis_fill_buffer(void);
void vorbis_stop(void);
void vorbis_pause(void);
void vorbis_resume(void);
int vorbis_isplaying(void);
int vorbis_get_samplerate(VORBIS *vorbis);
int vorbis_get_channels(VORBIS *vorbis);
int vorbis_get_volume(void);
void vorbis_set_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif  /* ifndef ALPORT_VORBIS_H */
