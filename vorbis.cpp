//#include <string.h>
#include <stdlib.h>
#include "alport.h"
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_MAX_CHANNELS  2
#include "stbvorbis/stb_vorbis.h"



#define VORBIS_BIT_DEPTH 16 /* Standard Bit depth for a VORBIS */


static stb_vorbis *_vorbis = NULL;
static SAMPLE *_vorbisSpl = NULL; /* sample object to pass VORBIS output to the mixer */
static float _delta = -1.0;
static int _mvoice = -1; /* voice index in the mixer */
static unsigned int _render_size; /* audio frame count to render at a time */
static int _loop = FALSE;
static int _playing = FALSE;


/* vorbis_load:
 *  Load VORBIS file from path location using Allegro's packfile
 * routines. It returns a pointer to a VORBIS object
 * that can be used by the VORBIS playing routines.
 */
VORBIS *vorbis_load(const char *filename)
{
   PACKFILE *f = NULL;
   long size = 0;
   void *buf = NULL;
   stb_vorbis *vorbis = NULL;

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

   /* Load it from buffer and get the stb_vorbis */
   vorbis = (stb_vorbis *)vorbis_create(buf, size);
   /* Set buf to NULL so it doesn't get freed below,
      as it will be later released together with the stb_vorbis. */
   if (vorbis)
      buf = NULL;

_RETURN:
   if (buf)
      free(buf);
   if (f)
      pack_fclose(f);

   return vorbis;
}


/* vorbis_create:
 *  Reads VORBIS data from a buffer, it creates the stb_vorbis
 * structure using the passed buffer and size.
 * Return NULL if any error is encountered.
 */
VORBIS *vorbis_create(void *data, size_t data_len)
{
   /* Set some params as NULL to use defaults */
   stb_vorbis *vorbis = stb_vorbis_open_memory((const unsigned char *)data, data_len, NULL, NULL);
   if (!vorbis)
      return NULL;

   return vorbis;
}


/* vorbis_destroy:
 *  Frees the memory being used by a VORBIS object. If passed TRUE
 * in the free_buf parameter, it will also freed the original
 * VORBIS file buffer.
 */
void vorbis_destroy(VORBIS *vorbis, int free_buf)
{
   stb_vorbis *_stbvorbis = (stb_vorbis *)vorbis;

   if (_stbvorbis == NULL)
      return;

   /* Free the original vorbis file buffer */
   if (free_buf && _stbvorbis->stream_start)
      free((void *)(_stbvorbis->stream_start));

   stb_vorbis_close(_stbvorbis);
}


/* vorbis_init:
 *  Setup the VORBIS engine to be used together with the mixer.
 * delta: Ratio at which a sound slice will be queried i.e. 1/60
 */
int vorbis_init(float delta)
{
   /* Already initialized */
   if (_delta > 0.0)
      return FALSE;

   _delta = delta;
   _vorbis = NULL;
   _playing = FALSE;

   return TRUE;
}


/* vorbis_deinit:
 *  Resets resources being used by the VORBIS engine.
 */
void vorbis_deinit(void)
{
   /* not initialized? */
   if (_delta <= 0.0)
      return;

   _delta = -1.0;
   _playing = FALSE;
   _vorbis = NULL;
}


/* vorbis_play:
 *  Sets a VORBIS object to the engine to start playing it.
 * if the VORBIS engine is not initiated yet or the VORBIS object is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise. It will make the VORBIS loop after finishing
 * playing if TRUE is passed in the loop parameter.
 */
int vorbis_play(VORBIS *vorbis, int loop)
{
   /* Has the engine been initiated and
    * the VORBIS object is valid? */
   if (_delta < 0.0 || !vorbis)
      return FALSE;

   /* if already playing something, stop it */
   if (_vorbis)
      vorbis_stop();

   _vorbis = (stb_vorbis *)vorbis;

   _render_size = _delta * _vorbis->sample_rate;

   /* Create a sample to store the output of VORBIS */
   _vorbisSpl = create_sample(VORBIS_BIT_DEPTH, (_vorbis->channels > 1 ? TRUE : FALSE), _vorbis->sample_rate, _render_size);
   if (!_vorbisSpl)
      return FALSE;

   /* Reserve voice for the VORBIS in the mixer */
   _mvoice = allocate_voice((const SAMPLE *)_vorbisSpl);

   _loop = loop;
   _playing = TRUE;

   return TRUE;
}


/* vorbis_stop:
 *  Stops VORBIS from being played and reset playing
 * control variables. To play again the same or another
 * VORBIS, a call to vorbis_play() is needed after calling this.
 */
void vorbis_stop(void)
{
   /* Do not continue if not playing */
   if (!_vorbis)
      return;

   deallocate_voice(_mvoice);
   _mvoice = -1;
   destroy_sample(_vorbisSpl);

   _vorbis = NULL;
   _playing = FALSE;
}


/* vorbis_fill_buffer:
 *  If a VORBIS is set for playing, it will call the
 * VORBIS engine to fill the buffer with the required audio frame count
 * ready for the audio mixer. The audio will loop or not 
 * depending on what was set on vorbis_play() call.
 */
void vorbis_fill_buffer(void)
{
   unsigned int i, framesRead;
   short *buf;

   if (!_playing)
      return;

   buf = (short *)_vorbisSpl->data;
   /* Get the rendered audio from the VORBIS engine into our sample buf */
   framesRead = stb_vorbis_get_samples_short_interleaved(_vorbis, _vorbis->channels, buf, _render_size * _vorbis->channels);

   /* reaching the end of the VORBIS? */
   if(framesRead < _render_size)
   {
      if (_loop)
      {
         /* rewind VORBIS */
         stb_vorbis_seek_start(_vorbis);
         /* Get the rest of the audio from the start to complete _render_size */
         stb_vorbis_get_samples_short_interleaved(_vorbis, _vorbis->channels, buf + (framesRead * _vorbis->channels), (_render_size - framesRead) * _vorbis->channels);
      }
      else
      {
         /* If some was read we need to fill in the gap with silence */
         if(framesRead > 0)
         {
            memset(buf + (framesRead * _vorbis->channels), 0, (_render_size - framesRead) * _vorbis->channels * sizeof(short));
         }
         else
         {
            /* No more to do, stop playing it and exit */
            vorbis_stop();
            return;
         }
      }
   }

   /* Convert to unsigned as required by the mixer */
   for (i = 0; i < (_render_size * _vorbis->channels); i++)
      buf[i] ^= 0x8000;

   /* queue the samples into the mixer */
   voice_start(_mvoice);
}


/* vorbis_get_samplerate:
 *  Returns HZ at which was the c encoded on
 * the passed VORBIS object. -1 if the passed object is
 * invalid.
 */
int vorbis_get_samplerate(VORBIS *vorbis)
{
   if(!vorbis)
   return -1;

   return ((stb_vorbis *)vorbis)->sample_rate;
}


/* vorbis_get_channels:
 *  Returns 1 if the VORBIS is mono or 2 if it is stereo.
 * If the passed VORBIS object is invalid it returns -1.
 */
int vorbis_get_channels(VORBIS *vorbis)
{
   if(!vorbis)
   return -1;
   
   return ((stb_vorbis *)vorbis)->channels;
}


/* vorbis_isplaying:
 *  Returns TRUE if a VORBIS is set for playing (even if
 * it has been paused), FALSE otherwise.
 */
int vorbis_isplaying(void)
{
   return (_vorbis != NULL);
}


/* vorbis_get_volume:
 *  Returns the actual volume used for VORBIS audio output.
 * It requires a VORBIS already playing to work.
 */
int vorbis_get_volume(void)
{
   /* not playing? */
   if (_mvoice < 0)
      return 0;

   return voice_get_volume(_mvoice);
}


/* vorbis_set_volume:
 *  Sets the actual volume to use for VORBIS audio output.
 * It needs an VORBIS already playing.
 */
void vorbis_set_volume(int volume)
{
   /* not playing? */
   if (_mvoice < 0)
      return;

   volume = CLAMP(0, volume, 255);
   voice_set_volume(_mvoice, volume);
}


/* vorbis_pause:
 *  Pauses VORBIS audio from playing for later resuming.
 * If already paused it does nothing.
 */
void vorbis_pause(void)
{
   _playing = FALSE;
}


/* vorbis_resume:
 *  Resumes VORBIS playing after being paused.
 * Does nothing if no VORBIS has been set for playing
 * using vorbis_play().
 */
void vorbis_resume(void)
{
   if (_vorbis)
      _playing = TRUE;
}
