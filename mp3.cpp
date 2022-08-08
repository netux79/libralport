#include <stdlib.h>
#include "alport.h"
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_ONLY_MP3
#include "drmp3/dr_mp3.h"

#define MP3_BIT_DEPTH 16 /* Standard Bit depth for an MP3 */

extern void *stream;
extern unsigned char stream_type;
extern SAMPLE *stream_sample;
extern float stream_delta;
extern int stream_voice;
extern int stream_playing;
extern int (*stream_audio_render)(short *buf, unsigned int render_size);
static int _loop = FALSE;


/* mp3_load:
 *  Load MP3 file from path location using Allegro's packfile
 * routines. It returns a pointer to a MP3 object
 * that can be used by the MP3 playing routines.
 */
MP3 *mp3_load(const char *filename)
{
   PACKFILE *f = NULL;
   long size = 0;
   void *buf = NULL;
   drmp3 *mp3 = NULL;

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

   /* Load it from buffer and get the drmp3 */
   mp3 = (drmp3 *)mp3_create(buf, size);
   /* Set buf to NULL so it doesn't get freed below,
      as it will be later released together with the drmp3. */
   if (mp3)
      buf = NULL;

_RETURN:
   if (buf)
      free(buf);
   if (f)
      pack_fclose(f);

   return mp3;
}


/* mp3_create:
 *  Reads MP3 data from a buffer, it creates the drmp3
 * structure using the passed buffer and size.
 * Return NULL if any error is encountered.
 */
MP3 *mp3_create(void *data, size_t data_len)
{
   drmp3 *mp3 = (drmp3 *)malloc(sizeof(drmp3));
   if (!mp3)
      return NULL;

   if (!drmp3_init_memory(mp3, data, data_len, NULL)) /* Set as NULL to use defaults */
   {
      free(mp3);
      return NULL;
   }

   return mp3;
}


/* mp3_destroy:
 *  Frees the memory being used by a MP3 object. If passed TRUE
 * in the free_buf parameter, it will also freed the original
 * mp3 file buffer.
 */
void mp3_destroy(MP3 *mp3, int free_buf)
{
   drmp3 *_drmp3 = (drmp3 *)mp3;

   if (_drmp3 == NULL)
      return;

   drmp3_uninit(_drmp3);

   /* Free the original mp3 file buffer */
   if (free_buf && _drmp3->memory.pData)
      free((void *)(_drmp3->memory.pData));

   free(_drmp3);
}


/* mp3_audio_render:
 *  If a MP3 is set for playing, it will call the
 * MP3 engine to fill the buffer with the required audio frame count
 * ready for the audio mixer. The audio will loop or not 
 * depending on what was set on mp3_play() call.
 */
int mp3_audio_render(short *buf, unsigned int render_size)
{
   unsigned int framesRead;
   drmp3 *mp3 = (drmp3 *)stream;

   /* Get the rendered audio from the MP3 engine into our sample buf */
   framesRead = drmp3_read_pcm_frames_s16(mp3, render_size, buf);

   /* reaching the end of the MP3? */
   if(framesRead < render_size)
   {
      if (_loop)
      {
         /* rewind MP3 */
         drmp3_seek_to_start_of_stream(mp3);
         /* Get the rest of the audio from the start to complete render_size */
         drmp3_read_pcm_frames_s16(mp3, render_size - framesRead, buf + (framesRead * mp3->channels));
      }
      else
      {
         /* If some was read we need to fill in the gap with silence */
         if(framesRead > 0)
            memset(buf + (framesRead * mp3->channels), 0, (render_size - framesRead) * mp3->channels * sizeof(short));
         else
            return FALSE; /* Nothing to render, exit */
      }
   }

   return TRUE;
}


/* stream_play_mp3:
 *  Sets a MP3 object to the engine to start playing it.
 * if the MP3 engine is not initiated yet or the MP3 object is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise. It will make the MP3 loop after finishing
 * playing if TRUE is passing in the loop parameter.
 */
int stream_play_mp3(MP3 *mp3, int loop)
{
   drmp3 *_mp3 = (drmp3 *)mp3;

   /* Has the engine been initiated and
    * the MP3 object is valid? */
   if (stream_delta < 0.0 || !_mp3)
      return FALSE;

   /* if already playing something, stop it */
   if (stream)
      stream_stop();

   /* Create a sample to store the output of MP3 */
   stream_sample = create_sample(MP3_BIT_DEPTH, (_mp3->channels > 1 ? TRUE : FALSE), _mp3->sampleRate, stream_delta * _mp3->sampleRate);
   if (!stream_sample)
      return FALSE;

   /* Reserve voice for the MP3 in the mixer */
   stream_voice = allocate_voice((const SAMPLE *)stream_sample);
   if (stream_voice == -1)
   {
      destroy_sample(stream_sample);
      return FALSE;
   }

   stream = _mp3;
   stream_type = STREAM_MP3;
   stream_audio_render = mp3_audio_render;
   _loop = loop;
   stream_playing = TRUE;

   return TRUE;
}


/* mp3_get_samplerate:
 *  Returns HZ at which was the MP3 encoded on
 * the passed MP3 object. -1 if the passed object is
 * invalid.
 */
int mp3_get_samplerate(MP3 *mp3)
{
   if(!mp3)
   return -1;

   return ((drmp3 *)mp3)->sampleRate;
}


/* mp3_get_channels:
 *  Returns 1 if the MP3 is mono or 2 if it is stereo.
 * If the passed MP3 object is invalid it returns -1.
 */
int mp3_get_channels(MP3 *mp3)
{
   if(!mp3)
   return -1;
   
   return ((drmp3 *)mp3)->channels;
}
