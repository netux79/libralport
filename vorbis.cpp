#include <stdlib.h>
#include "alport.h"
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_MAX_CHANNELS  2
#include "stbvorbis/stb_vorbis.h"

#define VORBIS_BIT_DEPTH 16 /* Standard Bit depth for a VORBIS */

extern void *stream;
extern unsigned char stream_type;
extern SAMPLE *stream_sample;
extern float stream_delta;
extern int stream_voice;
extern int stream_playing;
extern int (*stream_audio_render)(short *buf, unsigned int render_size);
static int _loop = FALSE;


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


/* vorbis_audio_render:
 *  If a VORBIS is set for playing, it will call this function
 * to fill the buffer with the required audio frame count
 * ready for the audio mixer. The audio will loop or not 
 * depending on what was set on vorbis_play() call.
 */
int vorbis_audio_render(short *buf, unsigned int render_size)
{
   unsigned int framesRead;
   stb_vorbis *vorbis = (stb_vorbis *)stream;

   /* Get the rendered audio from the VORBIS engine into our sample buf */
   framesRead = stb_vorbis_get_samples_short_interleaved(vorbis, vorbis->channels, buf, render_size * vorbis->channels);

   /* reaching the end of the VORBIS? */
   if(framesRead < render_size)
   {
      if (_loop)
      {
         /* rewind VORBIS */
         stb_vorbis_seek_start(vorbis);
         /* Get the rest of the audio from the start to complete render_size */
         stb_vorbis_get_samples_short_interleaved(vorbis, vorbis->channels, buf + (framesRead * vorbis->channels), (render_size - framesRead) * vorbis->channels);
      }
      else
      {
         /* If some was read we need to fill in the gap with silence */
         if(framesRead > 0)
            memset(buf + (framesRead * vorbis->channels), 0, (render_size - framesRead) * vorbis->channels * sizeof(short));
         else
            return FALSE; /* Nothing to render, exit */
      }
   }

   return TRUE;
}


/* stream_play_vorbis:
 *  Sets a VORBIS object to the engine to start playing it.
 * if the VORBIS engine is not initiated yet or the VORBIS object is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise. It will make the VORBIS loop after finishing
 * playing if TRUE is passed in the loop parameter.
 */
int stream_play_vorbis(VORBIS *vorbis, int loop)
{
   stb_vorbis *_vorbis = (stb_vorbis *)vorbis;

   /* Has the engine been initiated and
    * the VORBIS object is valid? */
   if (stream_delta < 0.0 || !_vorbis)
      return FALSE;

   /* if already playing something, stop it */
   if (stream)
      stream_stop();

   /* Create a sample to store the output of VORBIS */
   stream_sample = create_sample(VORBIS_BIT_DEPTH, (_vorbis->channels > 1 ? TRUE : FALSE), _vorbis->sample_rate, stream_delta * _vorbis->sample_rate);
   if (!stream_sample)
      return FALSE;

   /* Reserve voice for the VORBIS in the mixer */
   stream_voice = allocate_voice((const SAMPLE *)stream_sample);
   if (stream_voice == -1)
   {
      destroy_sample(stream_sample);
      return FALSE;
   }

   stream = _vorbis;
   stream_type = STREAM_VORBIS;
   stream_audio_render = vorbis_audio_render;
   _loop = loop;
   stream_playing = TRUE;

   return TRUE;
}


/* vorbis_get_samplerate:
 *  Returns HZ at which was the VORBIS encoded on
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
