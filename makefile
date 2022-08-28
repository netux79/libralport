# Linux makefile for allegroport

# where make install will put libalport.a
prefix=/usr/local
#prefix=.
#INCPATH=-I $(prefix)/include
#LIBPATH=$(prefix)/lib

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -pedantic -fPIC -std=gnu99 -O3
CXXFLAGS=-Wall -Wextra -pedantic -fPIC -std=gnu++11 -O3
LDFLAGS=

RANLIB=ranlib

OBJS = lzss.o \
	packfile.o \
	datafile.o \
	font.o \
	bitmap.o \
	primitive.o \
	primitive2.o \
	polygon.o \
	3d.o \
	rotate.o \
	palette.o \
	sound.o \
	midi.o \
	file.o \
	fix.o \
	stream.o \
	gme.o \
	mp3.o \
	vorbis.o \
	gme/abstract_file.o \
	gme/Blip_Buffer.o \
	gme/Classic_Emu.o \
	gme/Fir_Resampler.o \
	gme/Gb_Apu.o \
	gme/Gb_Cpu.o \
	gme/Gb_Oscs.o \
	gme/Gbs_Emu.o \
	gme/Multi_Buffer.o \
	gme/Music_Emu.o \
	gme/Nes_Apu.o \
	gme/Nes_Cpu.o \
	gme/Nes_Fme7_Apu.o \
	gme/Nes_Namco_Apu.o \
	gme/Nes_Oscs.o \
	gme/Nes_Vrc6_Apu.o \
	gme/Nsf_Emu.o \
	gme/Snes_Spc.o \
	gme/Spc_Cpu.o \
	gme/Spc_Dsp.o \
	gme/Spc_Emu.o

all: libalport.a

libalport.a: $(OBJS)
	ar rcs $@  $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.o
	rm -f gme/*.o
	rm -f libalport.a

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
