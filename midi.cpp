#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "alport.h"
#define TSF_IMPLEMENTATION
#include "tsf/tsf.h"
#define TML_IMPLEMENTATION
#include "tsf/tml.h"

#ifndef MSB_FIRST
#define TO_B32(a)    SWAP32(a)
#define TO_B16(a)    SWAP16(a)
#else
#define TO_B32(a)    a
#define TO_B16(a)    a
#endif

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

static SAMPLE *_midiSpl; /* sample object to pass midi output to the mixer */
static tsf *_tinySF;
static tml_message *_tml = NULL; /* pointer to first midi message */
static tml_message *_tmlNext = NULL; /* next midi message to be played */
static float _mtime = -1.0f;
static float _delta;
static int _mvoice = -1; /* voice index in the mixer */
static int _rate;
static int _sample_size;
static int _loop = FALSE;
static int _loop_start = -1;
static int _loop_end = -1;
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
   memcpy(uc, "MThd", 4);     /* MIDI signature */
   ui = (unsigned int *)(uc + 4);
   *ui = TO_B32(6);           /* size of header chunk */
   us = (unsigned short *)(uc + 8);
   *us = TO_B16(1);           /* type 1 */
   us = (unsigned short *)(uc + 10);
   *us = TO_B16(num_tracks);  /* number of tracks */
   us = (unsigned short *)(uc + 12);
   *us = TO_B16(divisions);   /* beat divisions */

   /* Write the track data */
   uc += MIDI_HDR_SIZE;
   for (c = 0; c < MIDI_TRACKS; c++)
   {
      if (tr[c].len > 0)
      {
         memcpy(uc, "MTrk", 4);     /* Track signature*/
         ui = (unsigned int *)(uc + 4);
         *ui = TO_B32(tr[c].len);   /* track length */
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
   if (_mvoice >= 0)
      return FALSE;

   _rate = rate;
   _delta = delta;
   _sample_size = _rate * _delta;

   /* Load the SoundFont from a file */
   _tinySF = tsf_load_filename(sf2_path);
   if (!_tinySF)
      return FALSE;

   /* Set the SoundFont rendering output mode */
   tsf_set_output(_tinySF, TSF_STEREO_INTERLEAVED, _rate, VOLUME_GAIN);

   /* Create a sample to store the output of TSF */
   _midiSpl = create_sample(SAMPLE_BIT_DEPTH, TRUE, _rate, _sample_size);
   if (!_midiSpl)
   {
      tsf_close(_tinySF);
      _tinySF = NULL;
      return FALSE;
   }

   /* Reserve voice for the midi in the mixer */
   _mvoice = allocate_voice((const SAMPLE *)_midiSpl);

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
   if (_mvoice < 0)
      return;

   _playing = FALSE;
   _mtime = -1.0f;

   deallocate_voice(_mvoice);
   _mvoice = -1;

   destroy_sample(_midiSpl);
   tsf_close(_tinySF);
   _tinySF = NULL;
   if (_tml)
      tml_free(_tml);
   _tmlNext = _tml = NULL;
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
   if (_mvoice < 0 || !midi)
      return FALSE;

   /* release tml if a previous midi was on memory */
   if (_tml)
      tml_free(_tml);

   /* Stop all playing notes immediatly and reset all channel 
    * parameters. Initialize preset on special 10th MIDI channel 
    * to use percussion sound bank (128) if available */
   tsf_reset(_tinySF);
   tsf_channel_set_bank_preset(_tinySF, 9, 128, 0);

   /* Normally we need to pass the buffer size but in this case we 
    * cheat since we don't have this value at this point 
    * and it is not really used */
   _tml = tml_load_memory(midi, MAX_MIDI_SIZE);
   if (!_tml)
      return FALSE;

   /* Set up the midi message pointer to the first MIDI message */
   _tmlNext = _tml;

   _mtime = 0.0f;
   _loop = loop;
   _loop_start = -1;
   _loop_end = -1;
   _playing = TRUE;

   return TRUE;
}


/* midi_stop:
 *  Stop midi music from being played and reset playing
 * control variables.
 */
void midi_stop(void)
{
   if (_mtime < 0.0f)
      return;

   _mtime = -1.0f;
   _loop = FALSE;
   _playing = FALSE;
   _tmlNext = NULL;
}


/* process_midi_msg:
 *  Process the current midi msg depending its type. If apply_note is false
 * it skips the notes processing. Useful for the Fast Forward functionality.
 */
static void process_midi_msg(int apply_note)
{
   switch (_tmlNext->type)
   {
      case TML_PROGRAM_CHANGE: /* channel program (preset) change (special 
                                * handling for 10th MIDI channel with 
                                * drums) */
         tsf_channel_set_presetnumber(_tinySF, _tmlNext->channel, 
                                      _tmlNext->program, 
                                      (_tmlNext->channel == 9));
         break;
      case TML_NOTE_ON: /* play a note, skip on FF */
         if (apply_note)
            tsf_channel_note_on(_tinySF, _tmlNext->channel, _tmlNext->key, 
                                _tmlNext->velocity / 127.0f);
         break;
      case TML_NOTE_OFF: /* stop a note, skip on FF */
         if (apply_note)
            tsf_channel_note_off(_tinySF, _tmlNext->channel, _tmlNext->key);
         break;
      case TML_PITCH_BEND: /* pitch wheel modification */
         tsf_channel_set_pitchwheel(_tinySF, _tmlNext->channel,
                                    _tmlNext->pitch_bend);
         break;
      case TML_CONTROL_CHANGE: /* MIDI controller messages */
         tsf_channel_midi_control(_tinySF, _tmlNext->channel,
                                  _tmlNext->control, _tmlNext->control_value);
         break;
   }
}


/* midi_fastforward:
 *  Process the midi messages accordingly up to the target beat position.
 * It skips notes messages to avoid any sound being applied. If -1 or 0 is
 * provided as target or no midi is currently playing, no work will be done.
 */
void midi_fastforward(int target)
{
   if (_mtime < 0.0f)
      return;

   /* Loop through all MIDI messages in the list until the target 
    * beat number is reached */
   for (; _tmlNext && _tmlNext->beat_no < target; _tmlNext = _tmlNext->next, 
                                                _mtime = _tmlNext->time)
      process_midi_msg(FALSE);
}


/* midi_render:
 *  Process the midi messages accordingly to the requested frameCount
 * and generated the audio into the provided buffer.
 * If we reach the end of the song or the defined loop end, finish
 * the processing.
 */
static void midi_render(short *buf, int frameCount)
{
   int sampleBlock;

   for (sampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; frameCount; 
        frameCount -= sampleBlock, buf += (sampleBlock * AUDIO_CHANNELS))
   {
      if (sampleBlock > frameCount) sampleBlock = frameCount;

      /* Loop through all MIDI messages which need to be played up 
       * until the current playback time */
      for (_mtime += sampleBlock * (1000.0 / _rate);
           _tmlNext && _mtime >= _tmlNext->time;
           _tmlNext = _tmlNext->next)
      {
         /* stop processing if loop end is reached */
         if (_tmlNext->beat_no == _loop_end)
         {
            _tmlNext = NULL; /* set to null to mark it as finished */
            break;
         }

         process_midi_msg(TRUE);
      }

      /* Render the block of audio samples in short format */
      tsf_render_short(_tinySF, buf, sampleBlock, 0);
   }
}


/* midi_fill_buffer:
 *  If a midi is set for playing, it will call the
 * midi sequencer and fill the buffer with the data
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
   if (!_tmlNext)
   {
      if (_loop)
      {
         _mtime = 0.0f;
         _tmlNext = _tml; /* point again to the start message  */

         /* fast forward the song if loop start was set */
         if (_loop_start > 0)
         {
            tsf_note_off_all(_tinySF);
            midi_fastforward(_loop_start);
         }
      }
      else
      {
         midi_stop();
         return;
      }
   }

   buf = (short *)_midiSpl->data;
   midi_render(buf, _sample_size);

   /* Convert to unsigned as required by the mixer */
   for (i = 0; i < _sample_size * AUDIO_CHANNELS; i++)
      buf[i] ^= 0x8000;

   /* queue the samples into the mixer */
   voice_start(_mvoice);
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
 *  Resumes music play after being paused.
 * Does nothing if no midi has been set for playing
 * using play_midi().
 */
void midi_resume(void)
{
   if (_mtime >= 0.0f)
      _playing = TRUE;
}


/* midi_loopstart:
 *  Sets the loop start within the midi song.
 * -1 means the original start of the midi.
 * Does nothing if no midi has been set for playing.
 */
void midi_loopstart(int value)
{
   if (_mtime >= 0.0f)
      _loop_start = value;
}


/* midi_loopend:
 *  Sets the loop end within the midi song.
 * -1 means the original end of the midi.
 * Does nothing if no midi has been set for playing.
 */
void midi_loopend(int value)
{
   if (_mtime >= 0.0f)
      _loop_end = value;
}


/* midi_isplaying:
 *  Returns TRUE if a midi is set for playing (even if
 * it has been paused), FALSE otherwise.
 */
int midi_isplaying(void)
{
   return (_mtime >= 0.0f);
}


/* midi_get_volume:
 *  Returns the actual music volume used for midi output.
 */
int midi_get_volume(void)
{
   /* not initialized? */
   if (_mvoice < 0)
      return 0;

   return voice_get_volume(_mvoice);
}

/* midi_set_volume:
 *  Returns the actual music volume used for midi output.
 */
void midi_set_volume(int volume)
{
   /* not initialized? */
   if (_mvoice < 0)
      return;

   volume = CLAMP(0, volume, 255);
   voice_set_volume(_mvoice, volume);
}
