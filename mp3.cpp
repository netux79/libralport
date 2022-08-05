//#include <string.h>
#include <stdlib.h>
#include "alport.h"
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_ONLY_MP3
#include "drmp3/dr_mp3.h"


#define MP3_BIT_DEPTH 16 /* Standard Bit depth for an MP3 */


static drmp3 *_mp3 = NULL;
static SAMPLE *_mp3Spl = NULL; /* sample object to pass MP3 output to the mixer */
static float _delta = -1.0;
static int _mvoice = -1; /* voice index in the mixer */
static unsigned int _render_size; /* audio frame count to render at a time */
static int _loop = FALSE;
static int _playing = FALSE;


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


/* mp3_init:
 *  Setup the MP3 engine to be used together with the mixer.
 * delta: Ratio at which a sound slice will be queried i.e. 1/60
 */
int mp3_init(float delta)
{
   /* Already initialized */
   if (_delta > 0.0)
      return FALSE;

   _delta = delta;
   _mp3 = NULL;
   _playing = FALSE;

   return TRUE;
}


/* mp3_deinit:
 *  Resets resources being used by the engine.
 */
void mp3_deinit(void)
{
   /* not initialized? */
   if (_delta <= 0.0)
      return;

   _delta = -1.0;
   _playing = FALSE;
   _mp3 = NULL;
}


/* mp3_play:
 *  Sets a MP3 object to the engine to start playing it.
 * if the MP3 engine is not initiated yet or the MP3 object is
 * null it does nothing. Returns TRUE if successful or
 * zero (FALSE) otherwise. It will make the MP3 loop after finishing
 * playing if TRUE is passing in the loop parameter.
 */
int mp3_play(MP3 *mp3, int loop)
{
   /* Has the engine been initiated and
    * the MP3 object is valid? */
   if (_delta < 0.0 || !mp3)
      return FALSE;

   /* if already playing something, stop it */
   if (_mp3)
      mp3_stop();

   _mp3 = (drmp3 *)mp3;

   _render_size = _delta * _mp3->sampleRate;

   /* Create a sample to store the output of MP3 */
   _mp3Spl = create_sample(MP3_BIT_DEPTH, (_mp3->channels > 1 ? TRUE : FALSE), _mp3->sampleRate, _render_size);
   if (!_mp3Spl)
      return FALSE;

   /* Reserve voice for the MP3 in the mixer */
   _mvoice = allocate_voice((const SAMPLE *)_mp3Spl);

   _loop = loop;
   _playing = TRUE;

   return TRUE;
}


/* mp3_stop:
 *  Stops MP3 from being played and reset playing
 * control variables. To play again the same or another
 * MP3, a call to mp3_play() is needed after calling this.
 */
void mp3_stop(void)
{
   /* Do not continue if not playing */
   if (!_mp3)
      return;

   deallocate_voice(_mvoice);
   _mvoice = -1;
   destroy_sample(_mp3Spl);

   _mp3 = NULL;
   _playing = FALSE;
}


/* mp3_fill_buffer:
 *  If a MP3 is set for playing, it will call the
 * MP3 engine to fill the buffer with the required audio frame count
 * ready for the audio mixer. The audio will loop or not 
 * depending on what was set on mp3_play() call.
 */
void mp3_fill_buffer(void)
{
   unsigned int i, framesRead;
   short *buf;

   if (!_playing)
      return;

   buf = (short *)_mp3Spl->data;
   /* Get the rendered audio from the MP3 engine into our sample buf */
   framesRead = drmp3_read_pcm_frames_s16(_mp3, _render_size, buf);

   /* reaching the end of the MP3? */
   if(framesRead < _render_size)
   {
      if (_loop)
      {
         /* rewind MP3 */
         drmp3_seek_to_start_of_stream(_mp3);
         /* Get the rest of the audio from the start to complete _render_size */
         drmp3_read_pcm_frames_s16(_mp3, _render_size - framesRead, buf + (framesRead * _mp3->channels));
      }
      else
      {
         /* If some was read we need to fill in the gap with silence */
         if(framesRead > 0)
         {
            memset(buf + (framesRead * _mp3->channels), 0, (_render_size - framesRead) * _mp3->channels * sizeof(short));
         }
         else
         {
            /* No more to do, stop playing it and exit */
            mp3_stop();
            return;
         }
      }
   }

   /* Convert to unsigned as required by the mixer */
   for (i = 0; i < (_render_size * _mp3->channels); i++)
      buf[i] ^= 0x8000;

   /* queue the samples into the mixer */
   voice_start(_mvoice);
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


/* mp3_isplaying:
 *  Returns TRUE if a MP3 is set for playing (even if
 * it has been paused), FALSE otherwise.
 */
int mp3_isplaying(void)
{
   return (_mp3 != NULL);
}


/* mp3_get_volume:
 *  Returns the actual volume used for MP3 audio output.
 * It requires a MP3 already playing to work.
 */
int mp3_get_volume(void)
{
   /* not initialized? */
   if (_mvoice < 0)
      return 0;

   return voice_get_volume(_mvoice);
}


/* mp3_set_volume:
 *  Sets the actual volume to use for MP3 audio output.
 * It needs an MP3 already playing.
 */
void mp3_set_volume(int volume)
{
   /* not initialized? */
   if (_mvoice < 0)
      return;

   volume = CLAMP(0, volume, 255);
   voice_set_volume(_mvoice, volume);
}


/* mp3_pause:
 *  Pauses MP3 audio from playing for later resuming.
 * If already paused it does nothing.
 */
void mp3_pause(void)
{
   _playing = FALSE;
}


/* mp3_resume:
 *  Resumes MP3 playing after being paused.
 * Does nothing if no MP3 has been set for playing
 * using mp3_play().
 */
void mp3_resume(void)
{
   if (_mp3)
      _playing = TRUE;
}
