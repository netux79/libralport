#include <stdlib.h>
#include <string.h>
#include "alport.h"


typedef struct MIXER_VOICE
{
   const SAMPLE *sample;   /* pointer to the original sample. */
   int playmode;           /* are we looping? */
   int autokill;           /* set to free the voice when the sample finishes */
   int vol;                /* current volume (fixed point .12) */
   int pan;                /* current pan (fixed point .12) */
   int freq;               /* current frequency (fixed point .12) */
   int playing;            /* are we active? */
   int channels;           /* # of channels for input data? */
   int bits;               /* sample bit-depth */
   union
   {
      unsigned char *u8;   /* data for 8 bit samples */
      unsigned short *u16; /* data for 16 bit samples */
      void *buffer;        /* generic data pointer */
   } data;
   long pos;               /* fixed point position in sample */
   long diff;              /* fixed point speed of play */
   long len;               /* fixed point sample length */
   long loop_start;        /* fixed point loop start position */
   long loop_end;          /* fixed point loop end position */
   int lvol;               /* left channel volume */
   int rvol;               /* right channel volume */
} MIXER_VOICE;


#define MIX_FIX_SHIFT      8 /* MIX_FIX_SHIFT must be <= (sizeof(int)*8)-24 */
#define MIX_FIX_SCALE      (1 << MIX_FIX_SHIFT)
#define UPDATE_FREQ        16
#define UPDATE_FREQ_SHIFT  4
#define MIX_VOLUME_LEVELS  32
#define VOICE_VOLUME_SCALE 1
#define MIX_CHANNELS    2

/* the samples currently being played */
static MIXER_VOICE mixer_voice[MIXER_MAX_SFX];

/* temporary sample mixing buffer */
static signed int *mix_buffer = NULL;

/* lookup table for converting sample volumes */
typedef signed int MIXER_VOL_TABLE[256];
static MIXER_VOL_TABLE mix_vol_table[MIX_VOLUME_LEVELS];

/* stats for the mixing code */
static int mix_voices;
static int mix_size;
static int mix_freq;
static int mix_quality;
static int mix_volume = 255;


/* load_sample_object:
 *  Loads a sample object from a datafile.
 */
void *load_sample_object(PACKFILE *f, long size)
{
   signed short bits;
   SAMPLE *s;

   (void)size;

   s = (SAMPLE *)malloc(sizeof(SAMPLE));
   if (!s)
      return NULL;

   bits = pack_mgetw(f);

   if (bits < 0)
   {
      s->bits = -bits;
      s->stereo = TRUE;
   }
   else
   {
      s->bits = bits;
      s->stereo = FALSE;
   }

   s->freq = pack_mgetw(f);
   s->len = pack_mgetl(f);
   s->loop_start = 0;
   s->loop_end = s->len;

   if (s->bits == 8)
      s->data = read_block(f, s->len * ((s->stereo) ? 2 : 1), 0);
   else
   {
      s->data = malloc(s->len * sizeof(short) * ((s->stereo) ? 2 : 1));
      if (s->data)
      {
         int i;

         for (i = 0; i < (int)s->len * ((s->stereo) ? 2 : 1); i++)
            ((unsigned short *)s->data)[i] = pack_igetw(f);

         if (pack_ferror(f))
         {
            free(s->data);
            s->data = NULL;
         }
      }
   }

   if (!s->data)
   {
      free(s);
      return NULL;
   }

   return s;
}


/* unload_sample:
 *  Destroys a sample object loaded from a datafile.
 */
void unload_sample(SAMPLE *s)
{
   if (s)
   {
      if (s->data)
         free(s->data);

      free(s);
   }
}


/* mixer_get_quality:
 *  Returns the current mixing quality, as loaded by the 'quality' param
 *  on the mixer_init function.
 */
int mixer_get_quality(void)
{
   return mix_quality;
}


/* mixer_get_frequency:
 *  Returns the mixer frequency, in Hz.
 */
int mixer_get_frequency(void)
{
   return mix_freq;
}


/* mixer_get_voices:
 *  Returns the number of voices allocated to the mixer.
 */
int mixer_get_voices(void)
{
   return mix_voices;
}


/* mixer_get_buffer_length:
 *  Returns the number of samples per channel in the mixer buffer.
 */
int mixer_get_buffer_length(void)
{
   return mix_size;
}


/* mixer_get_volume:
 * Retrieves the global sound output volume, as integers from 0 to 255.
 */
int mixer_get_volume()
{
   return mix_volume;
}


/* _clamp_val:
 *  Clamps an integer between 0 and the specified (positive!) value.
 */
static inline int _clamp_val(int i, int max)
{
   /* Clamp to 0 */
   i &= (~i) >> 31;

   /* Clamp to max */
   i -= max;
   i &= i >> 31;
   i += max;

   return i;
}


/* mixer_init:
 *  Initialises the sample mixing code, returning TRUE on success. You should
 *  pass it the number of samples you want it to mix each time the refill
 *  buffer routine is called, the sample rate to mix at, all the mix will
 *  be done in 16 bits and stereo. The bufsize parameter is the number of
 *  samples, not bytes. Take into account it is working in stereo so it
 *  means you may need to multiple by 2 for left and right channels.
 */
int mixer_init(int bufsize, int freq, int quality, int voices)
{
   int i, j;

   mix_quality = quality;
   if ((mix_quality < 0) || (mix_quality > 2))
      mix_quality = 2;

   mix_voices = voices;
   if (mix_voices > MIXER_MAX_SFX)
      mix_voices = MIXER_MAX_SFX;

   mix_freq = freq;
   mix_size = bufsize;
   if (mix_freq <=0 || mix_size <= 0)
      return FALSE;

   for (i = 0; i < MIXER_MAX_SFX; i++)
   {
      mixer_voice[i].playing = FALSE;
      mixer_voice[i].data.buffer = NULL;
   }

   /* temporary buffer for sample mixing */
   mix_buffer = (int *)malloc(mix_size * MIX_CHANNELS * sizeof(*mix_buffer));
   if (!mix_buffer)
   {
      mix_size = 0;
      mix_freq = 0;
      return FALSE;
   }

   for (j = 0; j < MIX_VOLUME_LEVELS; j++)
   {
      for (i = 0; i < 256; i++)
         mix_vol_table[j][i] = ((i - 128) * 256 * j / MIX_VOLUME_LEVELS) << 8;
   }

   return TRUE;
}


/* mixer_exit:
 *  Cleans up the sample mixer code when you are done with it.
 */
void mixer_exit(void)
{
   if (mix_buffer)
      free(mix_buffer);

   mix_buffer = NULL;
   mix_size = 0;
   mix_freq = 0;
   mix_voices = 0;
}


/* _update_volume_indexes:
 *  Called whenever the voice volume or pan changes, to update the mixer
 *  amplification table indexes.
 */
static void _update_volume_indexes(MIXER_VOICE *mv)
{
   int vol, pan, lvol, rvol;

   /* now use full 16 bit volume ranges */
   vol = mv->vol >> 12;
   pan = mv->pan >> 12;

   lvol = vol * (255 - pan);
   rvol = vol * pan;

   /* Adjust for 255*255 < 256*256-1 */
   lvol += lvol >> 7;
   rvol += rvol >> 7;

   /* Apply voice volume scale and clamp */
   mv->lvol = _clamp_val((lvol << 1) >> VOICE_VOLUME_SCALE, 65535);
   mv->rvol = _clamp_val((rvol << 1) >> VOICE_VOLUME_SCALE, 65535);

   if (!mix_quality)
   {
      /* Scale 16-bit -> table size */
      mv->lvol = mv->lvol * MIX_VOLUME_LEVELS / 65536;
      mv->rvol = mv->rvol * MIX_VOLUME_LEVELS / 65536;
   }
}


/* _update_voice_freq_rate:
 *  Called whenever the voice frequency changes, to update the sample
 *  delta value.
 */
static inline void _update_voice_freq_rate(MIXER_VOICE *mv)
{
   mv->diff = (mv->freq >> (12 - MIX_FIX_SHIFT)) / mix_freq;
}


/* helper for constructing the body of a sample mixing routine */
#define MIXER()                                                               \
{                                                                             \
   if ((spl->playmode & PLAYMODE_LOOP) && (spl->loop_start < spl->loop_end))  \
   {                                                                          \
      /* mix a forward looping sample */                                      \
      while (len--)                                                           \
      {                                                                       \
         MIX();                                                               \
         spl->pos += spl->diff;                                               \
         if (spl->pos >= spl->loop_end)                                       \
         {                                                                    \
            spl->pos -= (spl->loop_end - spl->loop_start);                    \
         }                                                                    \
      }                                                                       \
   }                                                                          \
   else                                                                       \
   {                                                                          \
      /* mix a non-looping sample */                                          \
      while (len--)                                                           \
      {                                                                       \
         MIX();                                                               \
         spl->pos += spl->diff;                                               \
         if ((unsigned long)spl->pos >= (unsigned long)spl->len)              \
         {                                                                    \
            spl->playing = FALSE;                                             \
            return;                                                           \
         }                                                                    \
      }                                                                       \
   }                                                                          \
}


/* mix_silent_samples:
 *  This is used when the voice is silent, instead of the other
 *  mix_*_samples() functions. It just extrapolates the sample position,
 *  and stops the sample if it reaches the end and isn't set to loop.
 *  Since no mixing is necessary, this function is much faster than
 *  its friends. In addition, no buffer parameter is required,
 *  and the same function can be used for all sample types.
 *
 *  There is a catch. All the mix_stereo_*_samples() and
 *  mix_hq?_*_samples() functions (those which write to a stereo mixing
 *  buffer) divide len by 2 before using it in the MIXER() macro.
 *  Therefore, all the mix_silent_samples() for stereo buffers must divide
 *  the len parameter by 2.
 */
static void mix_silent_samples(MIXER_VOICE *spl, int len)
{
   if ((spl->playmode & PLAYMODE_LOOP) && (spl->loop_start < spl->loop_end))
   {
      /* mix a forward looping sample */
      spl->pos += spl->diff * len;
      if (spl->pos >= spl->loop_end)
      {
         do
         {
            spl->pos -= (spl->loop_end - spl->loop_start);
         }
         while (spl->pos >= spl->loop_end);
      }
   }
   else
   {
      /* mix a non-looping sample */
      spl->pos += spl->diff * len;
      if ((unsigned long)spl->pos >= (unsigned long)spl->len)
      {
         spl->playing = FALSE;
         return;
      }
   }
}


/* mix_stereo_8x1_samples:
 *  Mixes from an eight bit sample into a stereo buffer, until either len
 *  samples have been mixed or until the end of the sample is reached.
 */
static void mix_stereo_8x1_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   signed int *lvol = (int *)(mix_vol_table + spl->lvol);
   signed int *rvol = (int *)(mix_vol_table + spl->rvol);

#define MIX()                                      \
   *(buf++) += lvol[spl->data.u8[spl->pos >> MIX_FIX_SHIFT]];  \
   *(buf++) += rvol[spl->data.u8[spl->pos >> MIX_FIX_SHIFT]];

   MIXER();

#undef MIX
}


/* mix_stereo_8x2_samples:
 *  Mixes from an eight bit stereo sample into a stereo buffer, until either
 *  len samples have been mixed or until the end of the sample is reached.
 */
static void mix_stereo_8x2_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   signed int *lvol = (int *)(mix_vol_table + spl->lvol);
   signed int *rvol = (int *)(mix_vol_table + spl->rvol);

#define MIX()                                                           \
   *(buf++) += lvol[spl->data.u8[(spl->pos >> MIX_FIX_SHIFT) * 2    ]]; \
   *(buf++) += rvol[spl->data.u8[(spl->pos >> MIX_FIX_SHIFT) * 2 + 1]];

   MIXER();

#undef MIX
}


/* mix_stereo_16x1_samples:
 *  Mixes from a 16 bit sample into a stereo buffer, until either len samples
 *  have been mixed or until the end of the sample is reached.
 */
static void mix_stereo_16x1_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   signed int *lvol = (int *)(mix_vol_table + spl->lvol);
   signed int *rvol = (int *)(mix_vol_table + spl->rvol);

#define MIX()                                                           \
   *(buf++) += lvol[(spl->data.u16[spl->pos >> MIX_FIX_SHIFT]) >> 8];   \
   *(buf++) += rvol[(spl->data.u16[spl->pos >> MIX_FIX_SHIFT]) >> 8];

   MIXER();

#undef MIX
}


/* mix_stereo_16x2_samples:
 *  Mixes from a 16 bit stereo sample into a stereo buffer, until either len
 *  samples have been mixed or until the end of the sample is reached.
 */
static void mix_stereo_16x2_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   signed int *lvol = (int *)(mix_vol_table + spl->lvol);
   signed int *rvol = (int *)(mix_vol_table + spl->rvol);

#define MIX()                                                                    \
   *(buf++) += lvol[(spl->data.u16[(spl->pos >> MIX_FIX_SHIFT) * 2    ]) >> 8];  \
   *(buf++) += rvol[(spl->data.u16[(spl->pos >> MIX_FIX_SHIFT) * 2 + 1]) >> 8];

   MIXER();

#undef MIX
}


/* mix_hq1_8x1_samples:
 *  Mixes from a mono 8 bit sample into a high quality stereo buffer,
 *  until either len samples have been mixed or until the end of the
 *  sample is reached.
 */
static void mix_hq1_8x1_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;

#define MIX()                                                           \
   *(buf++) += (spl->data.u8[spl->pos >> MIX_FIX_SHIFT] - 0x80) * lvol; \
   *(buf++) += (spl->data.u8[spl->pos >> MIX_FIX_SHIFT] - 0x80) * rvol;

   MIXER();

#undef MIX
}


/* mix_hq1_8x2_samples:
 *  Mixes from a stereo 8 bit sample into a high quality stereo buffer,
 *  until either len samples have been mixed or until the end of the
 *  sample is reached.
 */
static void mix_hq1_8x2_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;

#define MIX()                                                                    \
   *(buf++) += (spl->data.u8[(spl->pos >> MIX_FIX_SHIFT) * 2    ] - 0x80) * lvol;\
   *(buf++) += (spl->data.u8[(spl->pos >> MIX_FIX_SHIFT) * 2 + 1] - 0x80) * rvol;

   MIXER();

#undef MIX
}


/* mix_hq1_16x1_samples:
 *  Mixes from a mono 16 bit sample into a high-quality stereo buffer,
 *  until either len samples have been mixed or until the end of the sample
 *  is reached.
 */
static void mix_hq1_16x1_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;

#define MIX()                                                                    \
   *(buf++) += ((spl->data.u16[spl->pos >> MIX_FIX_SHIFT] - 0x8000) * lvol) >> 8;\
   *(buf++) += ((spl->data.u16[spl->pos >> MIX_FIX_SHIFT] - 0x8000) * rvol) >> 8;

   MIXER();

#undef MIX
}


/* mix_hq1_16x2_samples:
 *  Mixes from a stereo 16 bit sample into a high-quality stereo buffer,
 *  until either len samples have been mixed or until the end of the sample
 *  is reached.
 */
static void mix_hq1_16x2_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;

#define MIX()                                                                                \
   *(buf++) += ((spl->data.u16[(spl->pos >> MIX_FIX_SHIFT) * 2    ] - 0x8000) * lvol) >> 8;  \
   *(buf++) += ((spl->data.u16[(spl->pos >> MIX_FIX_SHIFT) * 2 + 1] - 0x8000) * rvol) >> 8;

   MIXER();

#undef MIX
}


/* Helper to apply a 16-bit volume to a 24-bit sample */
#define MULSC(a, b) ((int)((long long)((a) << 4) * ((b) << 12) >> 32))

/* mix_hq2_8x1_samples:
 *  Mixes from a mono 8 bit sample into an interpolated stereo buffer,
 *  until either len samples have been mixed or until the end of the
 *  sample is reached.
 */
static void mix_hq2_8x1_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;
   int v, v1, v2;

#define MIX()                                                                    \
   v = spl->pos >> MIX_FIX_SHIFT;                                                \
   v1 = (spl->data.u8[v] << 16) - 0x800000;                                      \
                                                                                 \
   if (spl->pos >= spl->len-MIX_FIX_SCALE)                                       \
   {                                                                             \
      if (spl->playmode & PLAYMODE_LOOP &&                                       \
          spl->loop_start < spl->loop_end && spl->loop_end == spl->len)          \
      {                                                                          \
         v2 = (spl->data.u8[spl->loop_start >> MIX_FIX_SHIFT] << 16) - 0x800000; \
      }                                                                          \
      else                                                                       \
      {                                                                          \
         v2 = 0;                                                                 \
      }                                                                          \
   }                                                                             \
   else                                                                          \
   {                                                                             \
      v2 = (spl->data.u8[v + 1] << 16) - 0x800000;                               \
   }                                                                             \
   v = spl->pos & (MIX_FIX_SCALE - 1);                                           \
   v = ((v2 * v) + (v1 * (MIX_FIX_SCALE - v))) >> MIX_FIX_SHIFT;                 \
                                                                                 \
   *(buf++) += MULSC(v, lvol);                                                   \
   *(buf++) += MULSC(v, rvol);

   MIXER();

#undef MIX
}


/* mix_hq2_8x2_samples:
 *  Mixes from a stereo 8 bit sample into an interpolated stereo buffer,
 *  until either len samples have been mixed or until the end of the
 *  sample is reached.
 */
static void mix_hq2_8x2_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;
   int v, va, v1a, v2a, vb, v1b, v2b;

#define MIX()                                                                                   \
   v = (spl->pos >> MIX_FIX_SHIFT) << 1; /* x2 for stereo */                                    \
   v1a = (spl->data.u8[v    ] << 16) - 0x800000;                                                \
   v1b = (spl->data.u8[v + 1] << 16) - 0x800000;                                                \
                                                                                                \
   if (spl->pos >= spl->len-MIX_FIX_SCALE)                                                      \
   {                                                                                            \
      if (spl->playmode & PLAYMODE_LOOP &&                                                      \
          spl->loop_start < spl->loop_end && spl->loop_end == spl->len)                         \
      {                                                                                         \
         v2a = (spl->data.u8[((spl->loop_start >> MIX_FIX_SHIFT) << 1)    ] << 16) - 0x800000;  \
         v2b = (spl->data.u8[((spl->loop_start >> MIX_FIX_SHIFT) << 1) + 1] << 16) - 0x800000;  \
      }                                                                                         \
      else                                                                                      \
      {                                                                                         \
         v2a = v2b = 0;                                                                         \
      }                                                                                         \
   }                                                                                            \
   else                                                                                         \
   {                                                                                            \
      v2a = (spl->data.u8[v + 2] << 16) - 0x800000;                                             \
      v2b = (spl->data.u8[v + 3] << 16) - 0x800000;                                             \
   }                                                                                            \
                                                                                                \
   v = spl->pos & (MIX_FIX_SCALE-1);                                                            \
   va = ((v2a * v) + (v1a * (MIX_FIX_SCALE - v))) >> MIX_FIX_SHIFT;                             \
   vb = ((v2b * v) + (v1b * (MIX_FIX_SCALE - v))) >> MIX_FIX_SHIFT;                             \
                                                                                                \
   *(buf++) += MULSC(va, lvol);                                                                 \
   *(buf++) += MULSC(vb, rvol);

   MIXER();

#undef MIX
}


/* mix_hq2_16x1_samples:
 *  Mixes from a mono 16 bit sample into an interpolated stereo buffer,
 *  until either len samples have been mixed or until the end of the sample
 *  is reached.
 */
static void mix_hq2_16x1_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;
   int v, v1, v2;

#define MIX()                                                                    \
   v = spl->pos >> MIX_FIX_SHIFT;                                                \
   v1 = (spl->data.u16[v] << 8) - 0x800000;                                      \
                                                                                 \
   if (spl->pos >= spl->len-MIX_FIX_SCALE)                                       \
   {                                                                             \
      if (spl->playmode & PLAYMODE_LOOP &&                                       \
          spl->loop_start < spl->loop_end && spl->loop_end == spl->len)          \
      {                                                                          \
         v2 = (spl->data.u16[spl->loop_start >> MIX_FIX_SHIFT] << 8) - 0x800000; \
      }                                                                          \
      else                                                                       \
      {                                                                          \
         v2 = 0;                                                                 \
      }                                                                          \
   }                                                                             \
   else                                                                          \
   {                                                                             \
      v2 = (spl->data.u16[v + 1] << 8) - 0x800000;                               \
   }                                                                             \
                                                                                 \
   v = spl->pos & (MIX_FIX_SCALE - 1);                                           \
   v = ((v2 * v) + (v1 * (MIX_FIX_SCALE - v))) >> MIX_FIX_SHIFT;                 \
                                                                                 \
   *(buf++) += MULSC(v, lvol);                                                   \
   *(buf++) += MULSC(v, rvol);

   MIXER();

#undef MIX
}


/* mix_hq2_16x2_samples:
 *  Mixes from a stereo 16 bit sample into an interpolated stereo buffer,
 *  until either len samples have been mixed or until the end of the sample
 *  is reached.
 */
static void mix_hq2_16x2_samples(MIXER_VOICE *spl, signed int *buf, int len)
{
   int lvol = spl->lvol;
   int rvol = spl->rvol;
   int v, va, v1a, v2a, vb, v1b, v2b;

#define MIX()                                                                                   \
   v = (spl->pos >> MIX_FIX_SHIFT) << 1; /* x2 for stereo */                                    \
   v1a = (spl->data.u16[v    ] << 8) - 0x800000;                                                \
   v1b = (spl->data.u16[v + 1] << 8) - 0x800000;                                                \
                                                                                                \
   if (spl->pos >= spl->len - MIX_FIX_SCALE)                                                    \
   {                                                                                            \
      if (spl->playmode & PLAYMODE_LOOP &&                                                      \
            spl->loop_start < spl->loop_end && spl->loop_end == spl->len)                       \
      {                                                                                         \
         v2a = (spl->data.u16[((spl->loop_start >> MIX_FIX_SHIFT) << 1)    ] << 8) - 0x800000;  \
         v2b = (spl->data.u16[((spl->loop_start >> MIX_FIX_SHIFT) << 1) + 1] << 8) - 0x800000;  \
      }                                                                                         \
      else                                                                                      \
      {                                                                                         \
         v2a = v2b = 0;                                                                         \
      }                                                                                         \
   }                                                                                            \
   else                                                                                         \
   {                                                                                            \
      v2a = (spl->data.u16[v + 2] << 8) - 0x800000;                                             \
      v2b = (spl->data.u16[v + 3] << 8) - 0x800000;                                             \
   }                                                                                            \
                                                                                                \
   v = spl->pos & (MIX_FIX_SCALE - 1);                                                          \
   va = ((v2a * v) + (v1a * (MIX_FIX_SCALE - v))) >> MIX_FIX_SHIFT;                             \
   vb = ((v2b * v) + (v1b * (MIX_FIX_SCALE - v))) >> MIX_FIX_SHIFT;                             \
                                                                                                \
   *(buf++) += MULSC(va, lvol);                                                                 \
   *(buf++) += MULSC(vb, rvol);

   MIXER();

#undef MIX
}


#define MAX_24 (0x00FFFFFF)

/* mixer_mix:
 *  Mixes samples into a buffer in memory, using the buffer size, sample
 *  frequency, etc, set when you called _mixer_init(). This should be
 *  called to get the next buffer full of samples.
 */
void mixer_mix(signed short *buf)
{
   signed int *p = mix_buffer;
   int i;

   /* clear mixing buffer */
   memset(p, 0, mix_size * MIX_CHANNELS * sizeof(*p));

   for (i = 0; i < mix_voices; i++)
   {
      if (mixer_voice[i].playing)
      {
         if (mixer_voice[i].vol > 0)
         {
            /* Interpolated mixing */
            if (mix_quality >= 2)
            {
               /* stereo input -> interpolated output */
               if (mixer_voice[i].channels != 1)
               {
                  if (mixer_voice[i].bits == 8)
                     mix_hq2_8x2_samples(mixer_voice + i, p, mix_size);
                  else
                     mix_hq2_16x2_samples(mixer_voice + i, p, mix_size);
               }
               /* mono input -> interpolated output */
               else
               {
                  if (mixer_voice[i].bits == 8)
                     mix_hq2_8x1_samples(mixer_voice + i, p, mix_size);
                  else
                     mix_hq2_16x1_samples(mixer_voice + i, p, mix_size);
               }
            }
            /* high quality mixing */
            else if (mix_quality)
            {
               /* stereo input -> high quality output */
               if (mixer_voice[i].channels != 1)
               {
                  if (mixer_voice[i].bits == 8)
                     mix_hq1_8x2_samples(mixer_voice + i, p, mix_size);
                  else
                     mix_hq1_16x2_samples(mixer_voice + i, p, mix_size);
               }
               /* mono input -> high quality output */
               else
               {
                  if (mixer_voice[i].bits == 8)
                     mix_hq1_8x1_samples(mixer_voice + i, p, mix_size);
                  else
                     mix_hq1_16x1_samples(mixer_voice + i, p, mix_size);
               }
            }
            /* low quality (fast?) stereo mixing */
            else
            {
               /* stereo input -> stereo output */
               if (mixer_voice[i].channels != 1)
               {
                  if (mixer_voice[i].bits == 8)
                     mix_stereo_8x2_samples(mixer_voice + i, p, mix_size);
                  else
                     mix_stereo_16x2_samples(mixer_voice + i, p, mix_size);
               }
               /* mono input -> stereo output */
               else
               {
                  if (mixer_voice[i].bits == 8)
                     mix_stereo_8x1_samples(mixer_voice + i, p, mix_size);
                  else
                     mix_stereo_16x1_samples(mixer_voice + i, p, mix_size);
               }
            }
         }
         else
            mix_silent_samples(mixer_voice + i, mix_size);
      }
   }

   /* transfer to the audio driver's buffer */
   for (i = mix_size * MIX_CHANNELS; i > 0; i--, buf++, p++)
      *buf = (_clamp_val((*p) + 0x800000, MAX_24) >> 8) ^ 0x8000;
}


/* mixer_init_voice:
 *  Initialises the specificed voice ready for playing a sample.
 */
static void mixer_init_voice(int voice, const SAMPLE *sample)
{
   mixer_voice[voice].sample = sample;
   mixer_voice[voice].autokill = FALSE;
   mixer_voice[voice].playmode = 0;
   mixer_voice[voice].vol = ((mix_volume >= 0) ? mix_volume : 255) << 12;
   mixer_voice[voice].pan = 128 << 12;
   mixer_voice[voice].freq = sample->freq << 12;
   mixer_voice[voice].playing = FALSE;
   mixer_voice[voice].channels = (sample->stereo ? 2 : 1);
   mixer_voice[voice].bits = sample->bits;
   mixer_voice[voice].pos = 0;
   mixer_voice[voice].len = sample->len << MIX_FIX_SHIFT;
   mixer_voice[voice].loop_start = sample->loop_start << MIX_FIX_SHIFT;
   mixer_voice[voice].loop_end = sample->loop_end << MIX_FIX_SHIFT;
   mixer_voice[voice].data.buffer = sample->data;

   _update_volume_indexes(mixer_voice + voice);
   _update_voice_freq_rate(mixer_voice + voice);
}


/* mixer_release_voice:
 *  Releases a voice when it is no longer required.
 */
static void mixer_release_voice(int voice)
{
   mixer_voice[voice].playing = FALSE;
   mixer_voice[voice].data.buffer = NULL;
}


/* find_available_voice:
 *  Looks for an available voice, killing off others if needed.
 */
static inline int find_available_voice()
{
   int c;

   /* look for a free voice */
   for (c = 0; c < mix_voices; c++)
   {
      if (!mixer_voice[c].sample)
         return c;
   }

   /* look for an autokill voice that has stopped */
   for (c = 0; c < mix_voices; c++)
   {
      if (mixer_voice[c].autokill && voice_get_position(c) < 0)
      {
         mixer_release_voice(c);
         return c;
      }
   }

   return -1;
}


/* allocate_voice:
 *  Allocates a voice ready for playing the specified sample, returning
 *  the voice number.
 *  Returns -1 if there is no voice available.
 */
int allocate_voice(const SAMPLE *spl)
{
   int voice;

   voice = find_available_voice();

   if (voice >= 0)
      mixer_init_voice(voice, spl);

   return voice;
}


/* create_sample:
 *  Constructs a new sample structure of the specified type.
 */
SAMPLE *create_sample(int bits, int stereo, int freq, int len)
{
   SAMPLE *spl;

   spl = (SAMPLE *)malloc(sizeof(SAMPLE));
   if (!spl)
      return NULL;

   spl->bits = bits;
   spl->stereo = stereo;
   spl->freq = freq;
   spl->len = len;
   spl->loop_start = 0;
   spl->loop_end = len;

   spl->data = malloc(len * ((bits == 8) ? 1 : sizeof(short)) * ((
                         stereo) ? 2 : 1));
   if (!spl->data)
   {
      free(spl);
      return NULL;
   }

   return spl;
}


/* destroy_sample:
 *  Frees a SAMPLE struct, checking whether the sample is currently playing,
 *  and stopping it if it is.
 */
void destroy_sample(SAMPLE *spl)
{
   int c;

   if (spl)
   {
      for (c = 0; c < mix_voices; c++)
      {
         if (mixer_voice[c].sample == spl)
            deallocate_voice(c);
      }

      if (spl->data)
         free(spl->data);

      free(spl);
   }
}


/* deallocate_voice:
 *  Releases a voice that was previously returned by allocate_voice().
 */
void deallocate_voice(int voice)
{
   if (mixer_voice[voice].sample)
   {
      voice_stop(voice);
      mixer_release_voice(voice);
      mixer_voice[voice].sample = NULL;
   }
}


/* mixer_set_volume:
 *  Alters the global sound output volume. Specify volumes for digital
 *  samples as integers from 0 to 255.
 */
void mixer_set_volume(int volume)
{
   int *voice_vol;
   int i;

   if (volume >= 0)
   {
      voice_vol = (int *)malloc(sizeof(int) * mix_voices);

      /* Retrieve the (relative) volume of each voice. */
      for (i = 0; i < mix_voices; i++)
         voice_vol[i] = voice_get_volume(i);

      mix_volume = CLAMP(0, volume, 255);

      /* Set the new (relative) volume for each voice. */
      for (i = 0; i < mix_voices; i++)
      {
         if (voice_vol[i] >= 0)
            voice_set_volume(i, voice_vol[i]);
      }

      free(voice_vol);
   }
}


/* reallocate_voice:
 *  Switches an already-allocated voice to use a different sample.
 */
void reallocate_voice(int voice, const SAMPLE *spl)
{
   if (voice >= 0)
   {
      voice_stop(voice);
      mixer_release_voice(voice);
      mixer_init_voice(voice, spl);
   }
}


/* release_voice:
 *  Flags that a voice is no longer going to be updated, so it can
 *  automatically be freed as soon as the sample finishes playing.
 */
void release_voice(int voice)
{
   mixer_voice[voice].autokill = TRUE;
}


/* voice_get_position:
 *  Returns the current play position of a voice, or -1 if that cannot
 *  be determined (because it has finished or been preempted by a
 *  different sound).
 */
int voice_get_position(int voice)
{
   if (mixer_voice[voice].sample)
   {
      if ((!mixer_voice[voice].playing)
            || (mixer_voice[voice].pos >= mixer_voice[voice].len))
         return -1;

      return (mixer_voice[voice].pos >> MIX_FIX_SHIFT);
   }
   else
      return -1;
}


/* voice_check:
 *  Checks whether a voice is playing, returning the sample if it is,
 *  or NULL if it has finished or been preempted by a different sound.
 */
SAMPLE *voice_check(int voice)
{
   if (mixer_voice[voice].sample)
   {
      if (mixer_voice[voice].autokill)
         if (voice_get_position(voice) < 0)
            return NULL;

      return (SAMPLE *)mixer_voice[voice].sample;
   }
   else
      return NULL;
}


/* voice_get_volume:
 *  Returns the current volume of a voice, or -1 if that cannot
 *  be determined (because it has finished or been preempted by a
 *  different sound).
 */
int voice_get_volume(int voice)
{
   int vol;

   if (mixer_voice[voice].sample)
      vol = mixer_voice[voice].vol >> 12;
   else
      vol = -1;

   if ((vol >= 0) && (mix_volume >= 0))
   {
      if (mix_volume > 0)
         vol = CLAMP(0, (vol * 255) / mix_volume, 255);
      else
         vol = 0;
   }

   return vol;
}


/* voice_set_volume:
 *  Sets the current volume of a voice.
 */
void voice_set_volume(int voice, int volume)
{
   if (mix_volume >= 0)
      volume = (volume * mix_volume) / 255;

   if (mixer_voice[voice].sample)
   {
      mixer_voice[voice].vol = volume << 12;
      _update_volume_indexes(mixer_voice + voice);
   }
}


/* voice_set_playmode:
 *  Sets the loopmode of a voice.
 */
void voice_set_playmode(int voice, int playmode)
{
   if (mixer_voice[voice].sample)
      mixer_voice[voice].playmode = playmode;
}


/* voice_set_position:
 *  Sets the play position of a voice.
 */
void voice_set_position(int voice, int position)
{
   if (mixer_voice[voice].sample)
   {
      if (position < 0)
         position = 0;

      mixer_voice[voice].pos = (position << MIX_FIX_SHIFT);
      if (mixer_voice[voice].pos >= mixer_voice[voice].len)
         mixer_voice[voice].playing = FALSE;
   }
}


/* voice_get_pan:
 *  Returns the current pan position of a voice, or -1 if that cannot
 *  be determined (because it has finished or been preempted by a
 *  different sound).
 */
int voice_get_pan(int voice)
{
   int pan;

   if (mixer_voice[voice].sample)
      pan = mixer_voice[voice].pan >> 12;
   else
      pan = -1;

   return pan;
}


/* voice_set_pan:
 *  Sets the pan position of a voice.
 */
void voice_set_pan(int voice, int pan)
{
   if (mixer_voice[voice].sample)
   {
      mixer_voice[voice].pan = pan << 12;
      _update_volume_indexes(mixer_voice + voice);
   }
}


/* voice_start:
 *  Activates a voice, with the currently selected parameters.
 */
void voice_start(int voice)
{
   if (mixer_voice[voice].sample)
   {
      mixer_voice[voice].pos = 0;
      mixer_voice[voice].playing = TRUE;
   }
}


/* voice_stop:
 *  Stops a voice from playing.
 */
void voice_stop(int voice)
{
   if (mixer_voice[voice].sample)
      mixer_voice[voice].playing = FALSE;
}
