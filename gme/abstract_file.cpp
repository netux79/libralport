
#include "abstract_file.h"

#include <assert.h>
#include <string.h>

/* Copyright (C) 2005 Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// to do: remove?
#ifndef RAISE_ERROR
	#define RAISE_ERROR( str ) return str
#endif

typedef Data_Reader::error_t error_t;

error_t Data_Reader::read( void* p, long s )
{
	long result = read_avail( p, s );
	if ( result != s )
	{
		if ( result >= 0 && result < s )
			RAISE_ERROR( "Unexpected end-of-file" );
		
		RAISE_ERROR( "Read error" );
	}
	
	return NULL;
}

error_t Data_Reader::skip( long count )
{
	char buf [512];
	while ( count )
	{
		int n = sizeof buf;
		if ( n > count )
			n = count;
		count -= n;
		RAISE_ERROR( read( buf, n ) );
	}
	return NULL;
}

long File_Reader::remain() const
{
	return size() - tell();
}

error_t File_Reader::skip( long n )
{
	assert( n >= 0 );
	if ( n )
		RAISE_ERROR( seek( tell() + n ) );
	
	return NULL;
}


// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	begin( (const char*) p ),
	pos( 0 ),
	size_( s )
{
}
	
long Mem_File_Reader::size() const {
	return size_;
}

long Mem_File_Reader::read_avail( void* p, long s )
{
	long r = remain();
	if ( s > r )
		s = r;
	memcpy( p, begin + pos, s );
	pos += s;
	return s;
}

long Mem_File_Reader::tell() const {
	return pos;
}

error_t Mem_File_Reader::seek( long n )
{
	if ( n > size_ )
		RAISE_ERROR( "Tried to go past end of file" );
	pos = n;
	return NULL;
}
