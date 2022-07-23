
// Abstract file access interfaces

#ifndef ABSTRACT_FILE_H
#define ABSTRACT_FILE_H

// Supports reading and finding out how many bytes are remaining
class Data_Reader {
public:
	Data_Reader() { }
	virtual ~Data_Reader() { }
	
	// NULL on success, otherwise error string
	typedef const char* error_t;
	
	// Read at most 'n' bytes. Return number of bytes read, zero or negative
	// if error.
	virtual long read_avail( void*, long n ) = 0;
	
	// Read exactly 'n' bytes (error if fewer are available).
	virtual error_t read( void*, long );
	
	// Number of bytes remaining
	virtual long remain() const = 0;
	
	// Skip forwards by 'n' bytes.
	virtual error_t skip( long n );
	
	// to do: bytes remaining = LONG_MAX when unknown?
	
private:
	// noncopyable
	Data_Reader( const Data_Reader& );
	Data_Reader& operator = ( const Data_Reader& );
};

// Adds seeking operations
class File_Reader : public Data_Reader {
public:
	// Size of file
	virtual long size() const = 0;
	
	// Current position in file
	virtual long tell() const = 0;
	
	// Change position in file
	virtual error_t seek( long ) = 0;
	
	virtual long remain() const;
	
	error_t skip( long n );
};

// Treat range of memory as a file
class Mem_File_Reader : public File_Reader {
	const char* const begin;
	long pos;
	const long size_;
public:
	Mem_File_Reader( const void*, long size );
	
	long size() const;
	long read_avail( void*, long );
	
	long tell() const;
	error_t seek( long );
};

#endif

