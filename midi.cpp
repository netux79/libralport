#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "alport.h"
#define TSF_IMPLEMENTATION
#include "tsf/tsf.h"
#define TML_IMPLEMENTATION
#include "tsf/tml.h"


#define BEL32(a)     ((a & 0xFF000000L) >> 24) | ((a & 0x00FF0000L) >> 8) | \
                     ((a & 0x0000FF00L) << 8) | ((a & 0x000000FFL) << 24)

#define BEW16(a)     ((a & 0xFF00L) >> 8) | ((a & 0x00FFL) << 8)

#define MIDI_TRACKS        32    /* Max tracks handled in allegro */
#define MIDI_HDR_SIZE      14
#define TRACK_HDR_SIZE     8
#define SAMPLE_BIT_DEPTH   16
#define VOLUME_GAIN        0.0f /* (>0 means higher, <0 means lower) */
#define AUDIO_CHANNELS     2
#define MAX_MIDI_SIZE      INT_MAX


typedef struct MIDI_TRACK
{
   unsigned char *data; /* MIDI track data */
   int len;             /* length of the track data */
} MIDI_TRACK;

static SAMPLE *midiSpl; /* sample object to pass midi output to the mixer */
static tsf *tinySF;
static tml_message *tml = NULL; /* pointer to first midi message */
static tml_message *tmlNext = NULL; /* next midi message to be played */
static float mtime = -1.0f;
static float _delta;
static int mvoice = -1; /* voice index in the mixer */
static int _rate;
static int _sample_size;
static int _loop = FALSE;
static int _playing = FALSE;

/* read_midi:
 *  Reads MIDI data from a packfile (in allegro MIDI format and
 * transform it to standard format to allow TSF engine processing).
 */
void *read_midi(PACKFILE *f)
{
   MIDI_TRACK *tr;
   int c;
   unsigned short divisions;
   int num_tracks = 0;
   unsigned int tracks_size = 0;
   void *m = NULL;
   unsigned char *uc;
   unsigned short *us;
   unsigned int *ui;

   tr = (MIDI_TRACK *)malloc(sizeof(MIDI_TRACK) * MIDI_TRACKS);
   if (!tr)
      return NULL;

   /* Read it as allegro MIDI format from file */
   divisions = pack_mgetw(f);

   for (c = 0; c < MIDI_TRACKS; c++)
   {
      tr[c].data = NULL; /* Clear data pointer */
      tr[c].len = pack_mgetl(f);
      if (tr[c].len > 0)
      {
         num_tracks++;
         tracks_size += (TRACK_HDR_SIZE + tr[c].len);
         tr[c].data = (unsigned char *)read_block(f, tr[c].len, 0);
         if (!tr[c].data)
            goto DEL_TEMP;
      }
   }

   /* Now restore it as standard MIDI format */
   m = malloc(MIDI_HDR_SIZE + tracks_size);
   if (!m)
      goto DEL_TEMP;

   /* Write MIDI header */
   uc = (unsigned char *)m;
   memcpy(uc, "MThd", 4);  /* MIDI signature */
   ui = (unsigned int *)(uc + 4);
   *ui = BEL32(6);         /* size of header chunk */
   us = (unsigned short *)(uc + 8);
   *us = BEW16(1);         /* type 1 */
   us = (unsigned short *)(uc + 10);
   *us = BEW16(num_tracks);/* number of tracks */
   us = (unsigned short *)(uc + 12);
   *us = BEW16(divisions); /* beat divisions */

   /* Write the track data */
   uc += MIDI_HDR_SIZE;
   for (c = 0; c < MIDI_TRACKS; c++)
   {
      if (tr[c].len > 0)
      {
         memcpy(uc, "MTrk", 4);  /* Track signature*/
         ui = (unsigned int *)(uc + 4);
         *ui = BEL32(tr[c].len); /* track length */
         uc += TRACK_HDR_SIZE;
         memcpy(uc, tr[c].data, tr[c].len);
         uc += tr[c].len;
      }
   }

DEL_TEMP:
   /* Clear temporary buffers. */
   if (tr)
   {
      for (c = 0; c < MIDI_TRACKS; c++)
      {
         if (tr[c].data)
            free(tr[c].data);
      }
      free(tr);
   }

   return m;
}


/* load_midi_object:
 *  Loads a midifile object from a datafile.
 */
void *load_midi_object(PACKFILE *f, long size)
{
   (void)size;

   return read_midi(f);
}


/* destroy_midi:
 *  Frees the memory being used by a MIDI file buffer.
 */
void destroy_midi(void *midi)
{
   if (midi)
      free(midi);
}


/* midi_init:
 *  Setup the midi engine to be used together with the mixer and
 * tsf / tml engine.
 */
int midi_init(int rate, float delta, const char *sf2_path)
{
   /* Already initialized */
   if (mvoice >= 0)
      return FALSE;

   _rate = rate;
   _delta = delta;
   _sample_size = _rate * _delta;

   /* Load the SoundFont from a file */
   tinySF = tsf_load_filename(sf2_path);
   if (!tinySF)
      return FALSE;

   /* Set the SoundFont rendering output mode */
   tsf_set_output(tinySF, TSF_STEREO_INTERLEAVED, _rate, VOLUME_GAIN);

   /* Create a sample to store the output of TSF */
   midiSpl = create_sample(SAMPLE_BIT_DEPTH, TRUE, _rate, _sample_size);
   if (!midiSpl)
   {
      tsf_close(tinySF);
      return FALSE;
   }

   /* Reserve voice for the midi in the mixer */
   mvoice = allocate_voice((const SAMPLE *)midiSpl);

   _playing = FALSE;

   return TRUE;
}


/* midi_deinit:
 *  Stop music and frees any resource being used by the midi
 * engine.
 */
void midi_deinit(void)
{
   /* not initialized? */
   if (mvoice < 0)
      return;

   _playing = FALSE;
   mtime = -1.0f;

   deallocate_voice(mvoice);
   mvoice = -1;

   destroy_sample(midiSpl);
   tsf_close(tinySF);
   if (tml)
      tml_free(tml);

   tmlNext = tml = NULL;
}


/* midi_play:
 *  Loads a midi file from buffer and starts playing it.
 * if the midi engine is not initiated yet or the buffer is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise. Doesn't behave exactly
 * like the allegro version.
 */
int midi_play(void *midi, int loop)
{

   /* Has the engine been initiated and
    * the midi buffer is valid? */
   if (mvoice < 0 || !midi)
      return FALSE;

   /* release tml if a previous midi was on memory */
   if (tml)
      tml_free(tml);

   /* Stop all playing notes immediatly and reset all channel 
    * parameters. Initialize preset on special 10th MIDI channel 
    * to use percussion sound bank (128) if available */
   tsf_reset(tinySF);
   tsf_channel_set_bank_preset(tinySF, 9, 128, 0);

   /* Normally we need to pass the buffer size but in this case we 
    * cheat since we don't have this value at this point 
    * and it is not really used */
   tml = tml_load_memory(midi, MAX_MIDI_SIZE);
   if (!tml)
      return FALSE;

   /* Set up the midi message pointer to the first MIDI message */
   tmlNext = tml;

   mtime = 0.0f;
   _loop = loop;
   _playing = TRUE;

   return TRUE;
}


/* midi_stop:
 *  Stop midi music from being played and reset playing
 * control variables.
 */
void midi_stop(void)
{
   if (mtime < 0.0f)
      return;

   mtime = -1.0f;
   _loop = FALSE;
   _playing = FALSE;
   tmlNext = NULL;
}


static void midi_render(short *buf, int frameCount)
{
   int sampleBlock;

   for (sampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; frameCount; 
        frameCount -= sampleBlock, buf += (sampleBlock * AUDIO_CHANNELS))
   {
      if (sampleBlock > frameCount) sampleBlock = frameCount;

      /* Loop through all MIDI messages which need to be played up 
       * until the current playback time */
      for (mtime += sampleBlock * (1000.0 / _rate);
           tmlNext && mtime >= tmlNext->time;
           tmlNext = tmlNext->next)
      {
         switch (tmlNext->type)
         {
            case TML_PROGRAM_CHANGE: /* channel program (preset) change (special 
                                      * handling for 10th MIDI channel with 
                                      * drums) */
               tsf_channel_set_presetnumber(tinySF, tmlNext->channel, 
                                            tmlNext->program, 
                                            (tmlNext->channel == 9));
               break;
            case TML_NOTE_ON: /* play a note */
               tsf_channel_note_on(tinySF, tmlNext->channel, tmlNext->key, 
                                   tmlNext->velocity / 127.0f);
               break;
            case TML_NOTE_OFF: /* stop a note */
               tsf_channel_note_off(tinySF, tmlNext->channel, tmlNext->key);
               break;
            case TML_PITCH_BEND: /* pitch wheel modification */
               tsf_channel_set_pitchwheel(tinySF, tmlNext->channel,
                                          tmlNext->pitch_bend);
               break;
            case TML_CONTROL_CHANGE: /* MIDI controller messages */
               tsf_channel_midi_control(tinySF, tmlNext->channel,
                                        tmlNext->control, tmlNext->control_value);
               break;
         }
      }

      /* Render the block of audio samples in short format */
      tsf_render_short(tinySF, buf, sampleBlock, 0);
   }
}


/* midi_fill_buffer:
 *  If a midi is set for playing, it will call the
 * midi sequencer and fill the buffer with the required
 * ready for the audio mixer. If the midi is set for
 * looping it will rewind and continue playing. Otherwise
 * it will stop playing automatically.
 */
void midi_fill_buffer(void)
{
   int i;
   short *buf;

   if (!_playing)
      return;

   /* reloop the song if needed */
   if (!tmlNext)
   {
      if (_loop)
      {
         mtime = 0.0f;
         tmlNext = tml; /* point again to the start message  */
      }
      else
      {
         midi_stop();
         return;
      }
   }

   buf = (short *)midiSpl->data;
   midi_render(buf, _sample_size);

   /* Convert to unsigned as required by the mixer */
   for (i = 0; i < _sample_size * AUDIO_CHANNELS; i++)
      buf[i] ^= 0x8000;

   /* queue the samples into the mixer */
   voice_start(mvoice);
}


/* midi_pause:
 *  Pauses music from playing for later resuming.
 * midi. If already paused it does nothing.
 */
void midi_pause(void)
{
   _playing = FALSE;
}


/* midi_resume:
 *  Resumes music from playing after being paused.
 * Does nothing if not midi has been set for playing
 * using play_midi().
 */
void midi_resume(void)
{
   if (mtime >= 0.0f)
      _playing = TRUE;
}


/* midi_isplaying:
 *  Returns TRUE if a midi is set for playing (even if
 * it has been paused), FALSE otherwise.
 */
int midi_isplaying(void)
{
   return (mtime >= 0.0f);
}


/* midi_get_volume:
 *  Returns the actual music volume used for midi output.
 */
int midi_get_volume(void)
{
   /* not initialized? */
   if (mvoice < 0)
      return 0;

   return voice_get_volume(mvoice);
}

/* midi_get_volume:
 *  Returns the actual music volume used for midi output.
 */
void midi_set_volume(int volume)
{
   /* not initialized? */
   if (mvoice < 0)
      return;

   volume = CLAMP(0, volume, 255);
   voice_set_volume(mvoice, volume);
}
