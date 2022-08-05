#ifndef ALPORT_MP3_H
#define ALPORT_MP3_H

#ifdef __cplusplus
extern "C" {
#endif


typedef void MP3;

MP3 *mp3_load(const char *filename);
MP3 *mp3_create(void *data, size_t data_len);
void mp3_destroy(MP3 *mp3, int free_buf);
int mp3_init(float delta);
void mp3_deinit(void);
int mp3_play(MP3 *mp3, int loop);
void mp3_fill_buffer(void);
void mp3_stop(void);
void mp3_pause(void);
void mp3_resume(void);
int mp3_isplaying(void);
int mp3_get_samplerate(MP3 *mp3);
int mp3_get_channels(MP3 *mp3);
int mp3_get_volume(void);
void mp3_set_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif  /* ifndef ALPORT_MP3_H */
