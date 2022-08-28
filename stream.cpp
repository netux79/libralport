#include <stdlib.h>
#include "alport.h"

void *stream = NULL;
unsigned char stream_type = STREAM_NONE;  /* Defines the current playing stream type */
SAMPLE *stream_sample = NULL; /* Sample object to pass audio output to the mixer */
float stream_delta = -1.0;    /* Ratio at which a sound slice will be queried i.e. 1/60 */
int stream_voice = -1;        /* Voice index in the mixer */
int stream_playing = FALSE;
int (*stream_audio_render)(short *buf, unsigned int render_size);


/* stream_init:
 *  Setup the STREAM engine to be used together with the mixer.
 * delta: Ratio at which a sound slice will be queried i.e. 1/60
 */
int stream_init(float delta)
{
   /* Already initialized */
   if (stream_delta > 0.0)
      return FALSE;

   stream_delta = delta;
   stream = NULL;
   stream_playing = FALSE;

   return TRUE;
}


/* stream_deinit:
 *  Resets resources being used by the STREAM engine.
 */
void stream_deinit(void)
{
   /* not initialized? */
   if (stream_delta <= 0.0)
      return;

   stream_delta = -1.0;
   stream_playing = FALSE;
   stream = NULL;
}


/* stream_stop:
 *  Stops STREAM from being played and reset playing
 * control variables. To play again the same or another
 * STREAM, a call to stream_play() is needed after calling this.
 */
void stream_stop(void)
{
   /* Do not continue if not playing */
   if (!stream)
      return;

   deallocate_voice(stream_voice);
   stream_voice = -1;
   destroy_sample(stream_sample);

   stream = NULL;
   stream_type = STREAM_NONE;
   stream_playing = FALSE;
}


/* stream_fill_buffer:
 *  If a STREAM is set for playing, it will call the
 * STREAM engine to fill the buffer with the required audio frame count
 * ready for the audio mixer. The audio will loop or not 
 * depending on what was set on stream_play() call.
 */
void stream_fill_buffer(void)
{
   unsigned int i;
   short *buf;

   if (!stream_playing)
      return;

   /* Get buffer from the Sample object to fill in */
   buf = (short *)stream_sample->data;

   /* Call audio render function */
   if (!stream_audio_render(buf, stream_sample->len))
   {
      /* If nothing to render we stop playing */
      stream_stop();
      return;
   }

   /* Convert to unsigned as required by the mixer */
   for (i = 0; i < (stream_sample->len * (stream_sample->stereo ? 2 : 1)); i++)
      buf[i] ^= 0x8000;

   /* queue the samples into the mixer */
   voice_start(stream_voice);
}


/* stream_get_samplerate:
 *  Returns HZ at which was the STREAM encoded on
 * the passed STREAM object. -1 if the passed object is
 * invalid.
 */
int stream_get_samplerate(void)
{
   if(!stream)
   return -1;

   return stream_sample->freq;
}


/* stream_get_channels:
 *  Returns 1 if the STREAM is mono or 2 if it is stereo.
 * If the passed STREAM object is invalid it returns -1.
 */
int stream_get_channels(void)
{
   if(!stream)
   return -1;
   
   return stream_sample->stereo ? 2 : 1;
}


/* stream_isplaying:
 *  Returns TRUE if a STREAM is set for playing (even if
 * it has been paused), FALSE otherwise.
 */
int stream_isplaying(void)
{
   return (stream != NULL);
}


/* stream_get_volume:
 *  Returns the actual volume used for STREAM audio output.
 * It requires a STREAM already playing to work.
 */
int stream_get_volume(void)
{
   /* not playing? */
   if (stream_voice < 0)
      return 0;

   return voice_get_volume(stream_voice);
}


/* stream_set_volume:
 *  Sets the actual volume to use for STREAM audio output.
 * It needs an STREAM already playing.
 */
void stream_set_volume(int volume)
{
   /* not playing? */
   if (stream_voice < 0)
      return;

   volume = CLAMP(0, volume, 255);
   voice_set_volume(stream_voice, volume);
}


/* stream_pause:
 *  Pauses STREAM audio from playing for later resuming.
 * If already paused it does nothing.
 */
void stream_pause(void)
{
   stream_playing = FALSE;
}


/* stream_resume:
 *  Resumes STREAM playing after being paused.
 * Does nothing if no STREAM has been set for playing
 * using stream_play().
 */
void stream_resume(void)
{
   if (stream)
      stream_playing = TRUE;
}


/* stream_get_type:
 *  Returns the actual playing STREAM type.
 * It requires a STREAM already playing to work.
 * If nothing is being played it returns STREAM_NONE.
 */
int stream_get_type(void)
{
   /* not playing? */
   if (stream_voice < 0)
      return STREAM_NONE;

   return stream_type;
}
