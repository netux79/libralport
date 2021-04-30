#ifndef midisequencer_hpp
#define midisequencer_hpp

#include <stdint.h>
#include <cstdio>
#include <string>
#include <vector>

#include "midisynth.h"

#define META_EVENT_ALL_NOTE_OFF     0x8888

namespace midisequencer
{
   /*
   typedef unsigned long uint_least32_t;
   */
   struct midi_message
   {
      float time;
      uint_least32_t message;
      int port;
      int track;
   };

   class uncopyable
   {
   public:
      uncopyable() {}
   private:
      uncopyable(const uncopyable &);
      void operator=(const uncopyable &);
   };

   class output: uncopyable
   {
   private:
      midisynth::synthesizer *synth;
      midisynth::fm_note_factory *note_factory;
      midisynth::DRUMPARAMETER p;

   public:
      output();
      void midi_message(int port, uint_least32_t message);
      void sysex_message(int port, const void *data, std::size_t size);
      void meta_event(int type, const void *data, std::size_t size);
      void reset();
      int synthesize(int_least16_t *output, std::size_t samples, float rate);
      void set_mode(midisynth::system_mode_t mode);
      ~output();
   };

   class sequencer: uncopyable
   {
   public:
      sequencer();
      void clear();
      void rewind();
      bool load(void *fp, int(*fgetc)(void *));
      bool load(std::FILE *fp);
      bool load(void *fp);
      int get_num_ports()const;
      float get_total_time()const;
      std::string get_title()const;
      std::string get_copyright()const;
      std::string get_song()const;
      void play(float time, output *out);
      void set_time(float time, output *out);
   private:
      std::vector<midi_message> messages;
      std::vector<midi_message>::iterator position;
      std::vector<std::string> long_messages;
      void load_smf(void *fp, int(*fgetc)(void *));
   };
}

#endif
