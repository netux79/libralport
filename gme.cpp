#include <stdlib.h>
#include "alport.h"
#include "gme/Gbs_Emu.h"
#include "gme/Nsf_Emu.h"
#include "gme/Spc_Emu.h"

#define GME_BIT_DEPTH   16
#define GME_CHANNELS     2

extern void *stream;
extern unsigned char stream_type;
extern SAMPLE *stream_sample;
extern float stream_delta;
extern int stream_voice;
extern int stream_playing;
extern int (*stream_audio_render)(short *buf, unsigned int render_size);


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


/* gme_audio_render:
 *  If a GME is set for playing, it will call this function
 * to fill the buffer with the required data slice
 * ready for the audio mixer. The audio will always loop for GME.
 * Returns FALSE if nothing was rendered.
 */
int gme_audio_render(short *buf, unsigned int render_size)
{
   Music_Emu *gme = (Music_Emu *)stream;

   gme->play(render_size * GME_CHANNELS, buf);

   return TRUE;
}


/* stream_play_gme:
 *  Sets a GME object to the engine to start playing it.
 * if the GME engine is not initiated yet or the GME object is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise.
 */
int stream_play_gme(GME *gme)
{
   Music_Emu* _gme = (Music_Emu*)gme;

   /* Has the engine and the GME object is valid? */
   if (stream_delta < 0.0 || !_gme)
      return FALSE;

   /* if already playing something, stop it */
   if (stream)
      stream_stop();

   /* Create the sample to store the output of GME */
   stream_sample = create_sample(GME_BIT_DEPTH, (GME_CHANNELS > 1 ? TRUE : FALSE), _gme->sample_rate(), stream_delta * _gme->sample_rate());
   if (!stream_sample)
      return FALSE;

   /* Reserve voice for the GME in the mixer */
   stream_voice = allocate_voice((const SAMPLE *)stream_sample);
   if (stream_voice == -1)
   {
      destroy_sample(stream_sample);
      return FALSE;
   }

   /* Reset GME object to its start position on first track */
   _gme->start_track(0);

   /* Sets it as the STREAM to be played */
   stream = _gme;
   stream_type = STREAM_GME;
   stream_audio_render = gme_audio_render;
   stream_playing = TRUE;

   return TRUE;
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
   Music_Emu* gme = (Music_Emu*)stream;
   
   if (!gme)
      return;

   if (stream_type != STREAM_GME)
      return;
   
   tracks = gme->track_count();
   track = CLAMP(0, track, tracks);

   gme->start_track(track);
}


/* gme_get_samplerate:
 *  Returns HZ at which was the GME encoded on
 * the passed GME object. -1 if the passed object is
 * invalid.
 */
int gme_get_samplerate(GME *gme)
{
   if (!gme)
      return -1;

   return ((Music_Emu *)gme)->sample_rate();
}


/* gme_get_channels:
 *  Returns 2 since all the GME are generated with 2 channels.
 * or -1 if the GME param is invalid.
 */
int gme_get_channels(GME *gme)
{
   if (!gme)
      return -1;

   return GME_CHANNELS;
}
