#include <string.h>
#include <stdlib.h>
#include "alport.h"
#include "gme/Gbs_Emu.h"
#include "gme/Nsf_Emu.h"
#include "gme/Spc_Emu.h"

#define SAMPLE_BIT_DEPTH   16
#define AUDIO_CHANNELS     2

static Music_Emu *_gme = NULL;
static SAMPLE *_gmeSpl = NULL; /* sample object to pass gme output to the mixer */
static float _delta = -1.0;
static int _mvoice = -1; /* voice index in the mixer */
static int _render_size; /* audio frame count to render at a time */
static int _playing = FALSE;


/* gme_load:
 *  Load GME file from path location using Allegro's packfile 
 * routines. It returns a pointer to a GME object 
 * that can be used by the GME playing routines.
 */
GME *gme_load(const char* filename)
{
   PACKFILE *f = NULL;
   long size = 0;
   char *ext = NULL;
   void *buf = NULL;
   GME_TYPE type = GME_TYPE::GME_NONE;
   Music_Emu *gme = NULL;

   size = file_size(filename);
   if (size <= 0)
      return NULL;

   f = pack_fopen(filename, F_READ);
   if (!f)
      return NULL;

   buf = malloc(size);
   if (!buf)
      goto _RETURN;

   if (pack_fread(buf, size, f) < size)
      goto _RETURN;

   /* Calculate the file type based on the extension */
   ext = get_extension(filename);
   if       (!strcmp(ext, "nsf") || !strcmp(ext, "NSF"))
      type = GME_TYPE::GME_NSF;
   else if  (!strcmp(ext, "gbs") || !strcmp(ext, "GBS"))
      type = GME_TYPE::GME_GBS;
   else if  (!strcmp(ext, "spc") || !strcmp(ext, "SPC"))
      type = GME_TYPE::GME_SPC;

   /* Load it from buffer and the proper music emulator */
   gme = (Music_Emu*)gme_create(buf, size, type);

_RETURN:
   if(buf) free(buf);
   if(f) pack_fclose(f);

   return gme;
}


/* gme_create:
 *  Reads GME data from a buffer, it creates the Music_Emu
 * emulator based on the passed GME_TYPE type.
 * Return NULL if any error is encountered.
 */
GME *gme_create(void* buf, size_t size, GME_TYPE type)
{
   int _rate;
   Music_Emu *gme = NULL;
   Mem_File_Reader reader(buf, size);

   if      (type == GME_TYPE::GME_NSF)
      gme = new Nsf_Emu;
   else if (type == GME_TYPE::GME_GBS)
      gme = new Gbs_Emu;
   else if (type == GME_TYPE::GME_SPC)
      gme = new Spc_Emu;
   else
      return NULL;

   /* Use the mixer sample rate to generate the GME output 
    * except for the SNES which native output is 32000 */
   _rate = (type == GME_TYPE::GME_SPC) ? 32000 : mixer_get_frequency();
   if (_rate <= 0)
      return NULL;

   /* Set the rate at which to play the GME */
   if (gme->set_sample_rate(_rate))
      return NULL;

   if(gme->load(reader))
      return NULL;

  return gme;
}


/* gme_destroy:
 *  Frees the memory being used by a GME emulator object.
 */
void gme_destroy(GME *gme)
{
   if (gme)
      delete (Music_Emu*)gme;
}


/* gme_init:
 *  Setup the GME engine to be used together with the mixer.
 * delta: Ratio at which a sound slice will be queried i.e. 1/60
 */
int gme_init(float delta)
{
   /* Already initialized */
   if (_delta > 0.0)
      return FALSE;

   _delta = delta;
   _gme = NULL;
   _playing = FALSE;

   return TRUE;
}


/* gme_deinit:
 *  Stop GME and frees any resource being used by the engine.
 */
void gme_deinit(void)
{
   /* not initialized? */
   if (_delta <= 0.0)
      return;

   _delta = -1.0;
   _playing = FALSE;
   _gme = NULL;
}


/* gme_play:
 *  Sets a GME object to the engine to start playing it.
 * if the GME engine is not initiated yet or the GME object is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise.
 */
int gme_play(GME *gme)
{
   /* Has the engine and the GME object is valid? */
   if (_delta < 0.0 || !gme)
      return FALSE;

   /* if already playing something, stop it */
   if (_gme)
      gme_stop();

   /* Sets the GME object to be played */
   _gme = (Music_Emu*)gme;

   _render_size = _delta * _gme->sample_rate();

   /* Create a sample to store the output of GME */
   _gmeSpl = create_sample(SAMPLE_BIT_DEPTH, TRUE, _gme->sample_rate(), _render_size);
   if (!_gmeSpl)
      return FALSE;

   /* Reserve voice for the GME in the mixer */
   _mvoice = allocate_voice((const SAMPLE *)_gmeSpl);

   /* Reset GME object to its start position on first track */
   _gme->start_track(0);

   _playing = TRUE;

   return TRUE;
}


/* gme_stop:
 *  Stops GME from being played and reset playing
 * control variables. To play again the same or another
 * GME, a call to gme_play() is needed after calling this.
 */
void gme_stop(void)
{
   /* Do not continue if not playing */
   if (!_gme)
      return;

   deallocate_voice(_mvoice);
   _mvoice = -1;
   destroy_sample(_gmeSpl);

   _gme = NULL;
   _playing = FALSE;
}


/* gme_fill_buffer:
 *  If a GME is set for playing, it will call the
 * GME engine to fill the buffer with the required data slice
 * ready for the audio mixer. The audio will always loop.
 */
void gme_fill_buffer(void)
{
   int i;
   short *buf;

   if (!_playing)
      return;

   buf = (short *)_gmeSpl->data;
   _gme->play(_render_size * AUDIO_CHANNELS, buf);

   /* Convert to unsigned as required by the mixer */
   for (i = 0; i < _render_size * AUDIO_CHANNELS; i++)
      buf[i] ^= 0x8000;

   /* queue the samples into the mixer */
   voice_start(_mvoice);
}


/* gme_pause:
 *  Pauses GME audio from playing for later resuming.
 * If already paused it does nothing.
 */
void gme_pause(void)
{
   _playing = FALSE;
}


/* gme_resume:
 *  Resumes GME playing after being paused.
 * Does nothing if no GME has been set for playing
 * using play_gme().
 */
void gme_resume(void)
{
   if (_gme)
      _playing = TRUE;
}


/* gme_isplaying:
 *  Returns TRUE if a GME is set for playing (even if
 * it has been paused), FALSE otherwise.
 */
int gme_isplaying(void)
{
   return (_gme != NULL);
}


/* gme_get_volume:
 *  Returns the actual volume used for GME audio output.
 */
int gme_get_volume(void)
{
   /* not playing? */
   if (_mvoice < 0)
      return 0;

   return voice_get_volume(_mvoice);
}


/* gme_set_volume:
 *  Sets the actual volume to use for GME audio output.
 */
void gme_set_volume(int volume)
{
   /* not playing? */
   if (_mvoice < 0)
      return;

   volume = CLAMP(0, volume, 255);
   voice_set_volume(_mvoice, volume);
}


/* gme_track_count:
 *  Gets the number of tracks on the passed GME audio.
 * Returns -1 if the passed GME is invalid.
 */
int gme_track_count(GME *gme)
{
   if (!gme)
      return -1;

   return ((Music_Emu *)gme)->track_count();
}


/* gme_change_track:
 *  Sets the track to be played on the actual GME audio.
 * It does nothing if no GME object has been set for playing.
 * If the track no is < 0, it will set the first track (0).
 * If the track is > the last track in the GME it will
 * set the last track for playing.
 */
void gme_change_track(int track)
{
   int tracks;
   
   if (!_gme)
      return;
   
   tracks = _gme->track_count();
   track = CLAMP(0, track, tracks);

   _gme->start_track(track);
}
