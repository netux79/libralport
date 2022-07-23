#ifndef ALPORT_GME_H
#define ALPORT_GME_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void GME;
enum GME_TYPE { GME_NSF, GME_GBS, GME_SPC, GME_NONE = 255 };


GME *load_gme(const char *filename);
GME *read_gme(void *buf, size_t size, GME_TYPE type);
void destroy_gme(GME *gme);
void gme_deinit(void);
void gme_fill_buffer(void);
int gme_get_volume(void);
int gme_track_count(GME *gme);
void gme_change_track(int track);
int gme_init(int rate, float delta);
int gme_isplaying(void);
void gme_pause(void);
int gme_play(GME *gme);
void gme_resume(void);
void gme_set_volume(int volume);
void gme_stop(void);

#ifdef __cplusplus
}
#endif

#endif          /* ifndef ALPORT_GME_H */
