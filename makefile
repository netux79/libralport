# Linux makefile for allegroport

# where make install will put libalport.a
prefix=/usr/local
#prefix=.
#INCPATH=-I $(prefix)/include
#LIBPATH=$(prefix)/lib

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -pedantic -fPIC -std=gnu++11 -O3 $(INCPATH)
LDFLAGS=

RANLIB=ranlib

OBJS = lzss.o packfile.o datafile.o font.o bitmap.o primitive.o palette.o sound.o midi.o \
		fmmidi/midisynth.o fmmidi/sequencer.o

all: libalport.a

libalport.a: $(OBJS)
	ar rcs $@  $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.o
	rm -f fmmidi/*.o
	rm -f libalport.a

%.o : %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@
