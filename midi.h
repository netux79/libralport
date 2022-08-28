#ifndef ALPORT_MIDI_H
#define ALPORT_MIDI_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

void destroy_midi(void *midi);
void *load_midi_object(PACKFILE *f, long size);
void midi_deinit(void);
void midi_fill_buffer(void);
int midi_get_volume(void);
int midi_init(int rate, float delta, const char *sf2_path);
int midi_isplaying(void);
void midi_pause(void);
int midi_play(void *midi, int loop);
void midi_fastforward(int target);
void midi_resume(void);
void midi_set_volume(int volume);
void midi_loopstart(int value);
void midi_loopend(int value);
void midi_stop(void);
void *read_midi(PACKFILE *f);

#ifdef __cplusplus
}
#endif

#endif          /* ifndef ALPORT_MIDI_H */
