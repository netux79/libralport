#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "alport.h"

#ifndef O_BINARY
#define O_BINARY  0
#endif

/* permissions to use when opening files */
/* some OSes have no concept of "group" and "other" */
#ifndef S_IRGRP
#define S_IRGRP   0
#define S_IWGRP   0
#endif
#ifndef S_IROTH
#define S_IROTH   0
#define S_IWOTH   0
#endif

#define OPEN_PERMS   (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)


static char the_password[256] = EMPTY_STRING;
static char pack_error = FALSE;
int _packfile_filesize = 0;
int _packfile_datasize = 0;
int _packfile_type = 0;


/* Signature of some functions declared later in this file */
static PACKFILE *pack_fopen_special_file(const char *filename,
      const char *mode);
static int normal_fclose(void *_f);
static int normal_getc(void *_f);
static int normal_ungetc(int ch, void *_f);
static int normal_putc(int c, void *_f);
static long normal_fread(void *p, long n, void *_f);
static long normal_fwrite(const void *p, long n, void *_f);
static int normal_fseek(void *_f, int offset);
static int normal_feof(void *_f);
static int normal_ferror(void *_f);
static int normal_refill_buffer(PACKFILE *f);
static int normal_flush_buffer(PACKFILE *f, int last);


static PACKFILE_VTABLE normal_vtable =
{
   normal_fclose,
   normal_getc,
   normal_ungetc,
   normal_fread,
   normal_putc,
   normal_fwrite,
   normal_fseek,
   normal_feof,
   normal_ferror
};


/* pack_fopen_datafile_object:
 *  Recursive helper to handle opening member objects from datafiles,
 *  given a fake filename in the form 'object_name[/nestedobject]'.
 */
static PACKFILE *pack_fopen_datafile_object(PACKFILE *f, const char *objname)
{
   char buf[512];   /* text is read into buf as UTF-8 */
   char name[512];
   int use_next = FALSE;
   int recurse = FALSE;
   int type, size, pos;
   char c;

   /* split up the object name */
   pos = 0;

   while ((c = objname[pos]) != 0)
   {
      if ((c == '#') || (c == '/') || (c == OTHER_PATH_SEPARATOR))
      {
         recurse = TRUE;
         break;
      }
      name[pos++] = c;
   }

   name[pos] = 0;

   pack_mgetl(f);

   /* search for the requested object */
   while (!pack_feof(f))
   {
      type = pack_mgetl(f);

      if (type == DAT_PROPERTY)
      {
         type = pack_mgetl(f);
         size = pack_mgetl(f);
         if (type == DAT_NAME)
         {
            /* examine name property */
            pack_fread(buf, size, f);
            buf[size] = 0;
            if (strcasecmp(buf, name) == 0)
               use_next = TRUE;
         }
         else
         {
            /* skip property */
            pack_fseek(f, size);
         }
      }
      else
      {
         if (use_next)
         {
            /* found it! */
            if (recurse)
            {
               if (type == DAT_FILE)
                  return pack_fopen_datafile_object(pack_fopen_chunk(f, FALSE), objname);
               else
                  break;
            }
            else
            {
               _packfile_type = type;
               return pack_fopen_chunk(f, FALSE);
            }
         }
         else
         {
            /* skip unwanted object */
            size = pack_mgetl(f);
            pack_fseek(f, size + 4);
         }
      }
   }

   /* oh dear, the object isn't there... */
   pack_fclose(f);
   pack_error = TRUE;
   return NULL;
}


/* pack_fopen_special_file:
 *  Helper to handle opening psuedo-files, ie. datafile objects and data
 *  that has been appended to the end of the executable.
 */
static PACKFILE *pack_fopen_special_file(const char *filename, const char *mode)
{
   char fname[1024], objname[512];
   PACKFILE *f;
   char *p;
   int c;

   /* special files are read-only */
   while ((c = *(mode++)) != 0)
   {
      if ((c == 'w') || (c == 'W'))
      {
         pack_error = TRUE;
         return NULL;
      }
   }

   if (*filename == '#')
   {
      /* read object from an appended datafile */
      strcpy(fname, "#");
      strcpy(objname, filename + 1);
   }
   else
   {
      /* read object from a regular datafile */
      strcpy(fname, filename);
      p = strrchr(fname, '#');
      *p = 0;
      strcpy(objname, p + 1);
   }

   /* open the file */
   f = pack_fopen(fname, F_READ_PACKED);
   if (!f)
      return NULL;

   if (pack_mgetl(f) != DAT_MAGIC)
   {
      pack_fclose(f);
      pack_error = TRUE;
      return NULL;
   }

   /* find the required object */
   return pack_fopen_datafile_object(f, objname);
}



/* packfile_password:
 *  Sets the password to be used by all future read/write operations.
 *  This only affects "normal" PACKFILEs, i.e. ones not using user-supplied
 *  packfile vtables.
 */
void packfile_password(const char *password)
{
   int i = 0;
   char c;

   if (password)
   {
      while ((c = password[i]) != 0)
      {
         the_password[i++] = c;
         if (i >= (int)sizeof(the_password) - 1)
            break;
      }
   }

   the_password[i] = 0;
}


/* encrypt_id:
 *  Helper for encrypting magic numbers, using the current password.
 */
static int32_t encrypt_id(long x, int new_format)
{
   int32_t mask = 0;
   int i, pos;

   if (the_password[0])
   {
      for (i = 0; the_password[i]; i++)
         mask ^= ((int32_t)the_password[i] << ((i & 3) * 8));

      for (i = 0, pos = 0; i < 4; i++)
      {
         mask ^= (int32_t)the_password[pos++] << (24 - i * 8);
         if (!the_password[pos])
            pos = 0;
      }

      if (new_format)
         mask ^= 42;
   }

   return x ^ mask;
}


/* clone_password:
 *  Sets up a local password string for use by this packfile.
 */
static int clone_password(PACKFILE *f)
{
   if (the_password[0])
   {
      if ((f->normal.passdata = strdup(the_password)) == NULL)
      {
         pack_error = TRUE;
         return FALSE;
      }
      f->normal.passpos = f->normal.passdata;
   }
   else
   {
      f->normal.passpos = NULL;
      f->normal.passdata = NULL;
   }

   return TRUE;
}


/* create_packfile:
 *  Helper function for creating a PACKFILE structure.
 */
static PACKFILE *create_packfile(int is_normal_packfile)
{
   PACKFILE *f;

   if (is_normal_packfile)
      f = (PACKFILE *)malloc(sizeof(PACKFILE));
   else
      f = (PACKFILE *)malloc(sizeof(PACKFILE) - sizeof(struct
                             _al_normal_packfile_details));

   if (f == NULL)
   {
      pack_error = TRUE;
      return NULL;
   }

   if (!is_normal_packfile)
   {
      f->vtable = NULL;
      f->userdata = NULL;
      f->is_normal_packfile = FALSE;
   }
   else
   {
      f->vtable = &normal_vtable;
      f->userdata = f;
      f->is_normal_packfile = TRUE;

      f->normal.buf_pos = f->normal.buf;
      f->normal.flags = 0;
      f->normal.buf_size = 0;
      f->normal.filename = NULL;
      f->normal.passdata = NULL;
      f->normal.passpos = NULL;
      f->normal.parent = NULL;
      f->normal.pack_data = NULL;
      f->normal.unpack_data = NULL;
      f->normal.todo = 0;
   }

   return f;
}


/* free_packfile:
 *  Helper function for freeing the PACKFILE struct.
 */
static void free_packfile(PACKFILE *f)
{
   if (f)
   {
      /* These are no longer the responsibility of this function, but
       * these assertions help catch instances of old code which still
       * rely on the old behaviour.
       */
      if (f->is_normal_packfile)
      {
      }

      free(f);
   }
}


/* _pack_fdopen:
 *  Converts the given file descriptor into a PACKFILE. The mode can have
 *  the same values as for pack_fopen() and must be compatible with the
 *  mode of the file descriptor. Unlike the libc fdopen(), pack_fdopen()
 *  is unable to convert an already partially read or written file (i.e.
 *  the file offset must be 0).
 *  On success, it returns a pointer to a file structure, and on error it
 *  returns NULL and stores an error code in errno. An attempt to read
 *  a normal file in packed mode will cause errno to be set to TRUE.
 */
PACKFILE *_pack_fdopen(int fd, const char *mode)
{
   PACKFILE *f, *f2;
   long header = FALSE;
   int c;

   if ((f = create_packfile(TRUE)) == NULL)
      return NULL;

   while ((c = *(mode++)) != 0)
   {
      switch (c)
      {
         case 'r':
         case 'R':
            f->normal.flags &= ~PACKFILE_FLAG_WRITE;
            break;
         case 'w':
         case 'W':
            f->normal.flags |= PACKFILE_FLAG_WRITE;
            break;
         case 'p':
         case 'P':
            f->normal.flags |= PACKFILE_FLAG_PACK;
            break;
         case '!':
            f->normal.flags &= ~PACKFILE_FLAG_PACK;
            header = TRUE;
            break;
      }
   }

   if (f->normal.flags & PACKFILE_FLAG_WRITE)
   {
      if (f->normal.flags & PACKFILE_FLAG_PACK)
      {
         /* write a packed file */
         f->normal.pack_data = create_lzss_pack_data();

         if (!f->normal.pack_data)
         {
            free_packfile(f);
            return NULL;
         }

         if ((f->normal.parent = _pack_fdopen(fd, F_WRITE)) == NULL)
         {
            free_lzss_pack_data(f->normal.pack_data);
            f->normal.pack_data = NULL;
            free_packfile(f);
            return NULL;
         }

         pack_mputl(encrypt_id(F_PACK_MAGIC, TRUE), f->normal.parent);

         f->normal.todo = 4;
      }
      else
      {
         /* write a 'real' file */
         if (!clone_password(f))
         {
            free_packfile(f);
            return NULL;
         }

         f->normal.hndl = fd;
         f->normal.todo = 0;

         errno = 0;

         if (header)
            pack_mputl(encrypt_id(F_NOPACK_MAGIC, TRUE), f);
      }
   }
   else
   {
      if (f->normal.flags & PACKFILE_FLAG_PACK)
      {
         /* read a packed file */
         f->normal.unpack_data = create_lzss_unpack_data();

         if (!f->normal.unpack_data)
         {
            free_packfile(f);
            return NULL;
         }

         if ((f->normal.parent = _pack_fdopen(fd, F_READ)) == NULL)
         {
            free_lzss_unpack_data(f->normal.unpack_data);
            f->normal.unpack_data = NULL;
            free_packfile(f);
            return NULL;
         }

         header = pack_mgetl(f->normal.parent);

         if ((f->normal.parent->normal.passpos) &&
               ((header == encrypt_id(F_PACK_MAGIC, FALSE)) ||
                (header == encrypt_id(F_NOPACK_MAGIC, FALSE))))
         {
            /* duplicate the file descriptor */
            int fd2 = dup(fd);

            if (fd2 < 0)
            {
               pack_fclose(f->normal.parent);
               free_packfile(f);
               pack_error = errno;
               return NULL;
            }

            /* close the parent file (logically, not physically) */
            pack_fclose(f->normal.parent);

            /* backward compatibility mode */
            if (!clone_password(f))
            {
               free_packfile(f);
               return NULL;
            }

            f->normal.flags |= PACKFILE_FLAG_OLD_CRYPT;

            /* re-open the parent file */
            lseek(fd2, 0, SEEK_SET);

            if ((f->normal.parent = _pack_fdopen(fd2, F_READ)) == NULL)
            {
               free_packfile(f);
               return NULL;
            }

            f->normal.parent->normal.flags |= PACKFILE_FLAG_OLD_CRYPT;

            pack_mgetl(f->normal.parent);

            if (header == encrypt_id(F_PACK_MAGIC, FALSE))
               header = encrypt_id(F_PACK_MAGIC, TRUE);
            else
               header = encrypt_id(F_NOPACK_MAGIC, TRUE);
         }

         if (header == encrypt_id(F_PACK_MAGIC, TRUE))
            f->normal.todo = LONG_MAX;
         else if (header == encrypt_id(F_NOPACK_MAGIC, TRUE))
         {
            f2 = f->normal.parent;
            free_lzss_unpack_data(f->normal.unpack_data);
            f->normal.unpack_data = NULL;
            free_packfile(f);
            return f2;
         }
         else
         {
            pack_fclose(f->normal.parent);
            free_lzss_unpack_data(f->normal.unpack_data);
            f->normal.unpack_data = NULL;
            free_packfile(f);
            pack_error = TRUE;
            return NULL;
         }
      }
      else
      {
         /* read a 'real' file */
         f->normal.todo = lseek(fd, 0, SEEK_END);  /* size of the file */
         if (f->normal.todo < 0)
         {
            pack_error = errno;
            free_packfile(f);
            return NULL;
         }

         lseek(fd, 0, SEEK_SET);

         if (!clone_password(f))
         {
            free_packfile(f);
            return NULL;
         }

         f->normal.hndl = fd;
      }
   }

   return f;
}


/* pack_fopen:
 *  Opens a file according to mode, which may contain any of the flags:
 *  'r': open file for reading.
 *  'w': open file for writing, overwriting any existing data.
 *  'p': open file in 'packed' mode. Data will be compressed as it is
 *       written to the file, and automatically uncompressed during read
 *       operations. Files created in this mode will produce garbage if
 *       they are read without this flag being set.
 *  '!': open file for writing in normal, unpacked mode, but add the value
 *       F_NOPACK_MAGIC to the start of the file, so that it can be opened
 *       in packed mode and Allegro will automatically detect that the
 *       data does not need to be decompressed.
 *
 *  Instead of these flags, one of the constants F_READ, F_WRITE,
 *  F_READ_PACKED, F_WRITE_PACKED or F_WRITE_NOPACK may be used as the second
 *  argument to fopen().
 *
 *  On success, fopen() returns a pointer to a file structure, and on error
 *  it returns NULL and stores an error code in errno. An attempt to read a
 *  normal file in packed mode will cause errno to be set to TRUE.
 */
PACKFILE *pack_fopen(const char *filename, const char *mode)
{
   int fd;

   _packfile_type = 0;

   if (strchr(filename, '#'))
   {
      PACKFILE *special = pack_fopen_special_file(filename, mode);
      if (special)
         return special;
   }

   if (strpbrk(mode, "wW"))  /* write mode? */
      fd = open(filename, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, OPEN_PERMS);
   else
      fd = open(filename, O_RDONLY | O_BINARY, OPEN_PERMS);

   if (fd < 0)
   {
      pack_error = errno;
      return NULL;
   }

   return _pack_fdopen(fd, mode);
}


/* pack_fclose:
 *  Closes a file after it has been read or written.
 *  Returns zero on success. On error it returns an error code which is
 *  also stored in errno. This function can fail only when writing to
 *  files: if the file was opened in read mode it will always succeed.
 */
int pack_fclose(PACKFILE *f)
{
   int ret;

   if (!f)
      return 0;

   ret = f->vtable->pf_fclose(f->userdata);
   if (ret != 0)
      pack_error = errno;

   free_packfile(f);

   return ret;
}


/* pack_fopen_chunk:
 *  Opens a sub-chunk of the specified file, for reading or writing depending
 *  on the type of the file. The returned file pointer describes the sub
 *  chunk, and replaces the original file, which will no longer be valid.
 *  When writing to a chunk file, data is sent to the original file, but
 *  is prefixed with two length counts (32 bit, big-endian). For uncompressed
 *  chunks these will both be set to the length of the data in the chunk.
 *  For compressed chunks, created by setting the pack flag, the first will
 *  contain the raw size of the chunk, and the second will be the negative
 *  size of the uncompressed data. When reading chunks, the pack flag is
 *  ignored, and the compression type is detected from the sign of the
 *  second size value. The file structure used to read chunks checks the
 *  chunk size, and will return EOF if you try to read past the end of
 *  the chunk. If you don't read all of the chunk data, when you call
 *  pack_fclose_chunk(), the parent file will advance past the unused data.
 *  When you have finished reading or writing a chunk, you should call
 *  pack_fclose_chunk() to return to your original file.
 */
PACKFILE *pack_fopen_chunk(PACKFILE *f, int pack)
{
   PACKFILE *chunk;

   /* unsupported */
   if (!f->is_normal_packfile)
   {
      pack_error = TRUE;
      return NULL;
   }

   if (f->normal.flags & PACKFILE_FLAG_WRITE)
   {

      /* write a sub-chunk */
      int tmp_fd = -1;
      char *tmp_dir = NULL;
      char *tmp_name = NULL;

      /* Get the path of the temporary directory */
      /* TODO: use current dir for now, look the libretro way later. */
      tmp_dir = strdup(".");

      /* the file is open in read/write mode, even if the pack file
       * seems to be in write only mode
       */
      tmp_name = (char *)malloc(strlen(tmp_dir) + 16);
      sprintf(tmp_name, "%s/XXXXXX", tmp_dir);
      tmp_fd = mkstemp(tmp_name);

      /* note: since the filename creation and the opening are not
       * an atomic operation, this is not secure
      char* tmpnam_string;
      tmpnam_string = tmpnam(NULL);
      tmp_name = malloc(strlen(tmp_dir) + strlen(tmpnam_string) + 2);
      sprintf(tmp_name, "%s/%s", tmp_dir, tmpnam_string);

      if (tmp_name) {
      tmp_fd = open(tmp_name, O_RDWR | O_BINARY | O_CREAT | O_EXCL, OPEN_PERMS);
      }*/


      if (tmp_fd < 0)
      {
         free(tmp_dir);
         free(tmp_name);

         return NULL;
      }

      chunk = _pack_fdopen(tmp_fd, (pack ? F_WRITE_PACKED : F_WRITE_NOPACK));

      if (chunk)
      {
         chunk->normal.filename = strdup(tmp_name);

         if (pack)
            chunk->normal.parent->normal.parent = f;
         else
            chunk->normal.parent = f;

         chunk->normal.flags |= PACKFILE_FLAG_CHUNK;
      }

      free(tmp_dir);
      free(tmp_name);
   }
   else
   {
      /* read a sub-chunk */
      _packfile_filesize = pack_mgetl(f);
      _packfile_datasize = pack_mgetl(f);

      if ((chunk = create_packfile(TRUE)) == NULL)
         return NULL;

      chunk->normal.flags = PACKFILE_FLAG_CHUNK;
      chunk->normal.parent = f;

      if (f->normal.flags & PACKFILE_FLAG_OLD_CRYPT)
      {
         /* backward compatibility mode */
         if (f->normal.passdata)
         {
            if ((chunk->normal.passdata = strdup(f->normal.passdata)) == NULL)
            {
               pack_error = TRUE;
               free(chunk);
               return NULL;
            }
            chunk->normal.passpos = chunk->normal.passdata + (size_t)f->normal.passpos -
                                    (size_t)f->normal.passdata;
            f->normal.passpos = f->normal.passdata;
         }
         chunk->normal.flags |= PACKFILE_FLAG_OLD_CRYPT;
      }

      if (_packfile_datasize < 0)
      {
         /* read a packed chunk */
         chunk->normal.unpack_data = create_lzss_unpack_data();

         if (!chunk->normal.unpack_data)
         {
            free_packfile(chunk);
            return NULL;
         }

         _packfile_datasize = -_packfile_datasize;
         chunk->normal.todo = _packfile_datasize;
         chunk->normal.flags |= PACKFILE_FLAG_PACK;
      }
      else
      {
         /* read an uncompressed chunk */
         chunk->normal.todo = _packfile_datasize;
      }
   }

   return chunk;
}


/* pack_fclose_chunk:
 *  Call after reading or writing a sub-chunk. This closes the chunk file,
 *  and returns a pointer to the original file structure (the one you
 *  passed to pack_fopen_chunk()), to allow you to read or write data
 *  after the chunk. If an error occurs, returns NULL and sets errno.
 */
PACKFILE *pack_fclose_chunk(PACKFILE *f)
{
   PACKFILE *parent;
   PACKFILE *tmp;
   char *name;
   int header, c;

   /* unsupported */
   if (!f->is_normal_packfile)
   {
      pack_error = TRUE;
      return NULL;
   }

   parent = f->normal.parent;
   name = f->normal.filename;

   if (f->normal.flags & PACKFILE_FLAG_WRITE)
   {
      /* finish writing a chunk */
      int hndl;

      /* duplicate the file descriptor to create a readable pack file,
       * the file descriptor must have been opened in read/write mode
       */
      if (f->normal.flags & PACKFILE_FLAG_PACK)
         hndl = dup(f->normal.parent->normal.hndl);
      else
         hndl = dup(f->normal.hndl);

      if (hndl < 0)
      {
         pack_error = errno;
         return NULL;
      }

      _packfile_datasize = f->normal.todo + f->normal.buf_size - 4;

      if (f->normal.flags & PACKFILE_FLAG_PACK)
      {
         parent = parent->normal.parent;
         f->normal.parent->normal.parent = NULL;
      }
      else
         f->normal.parent = NULL;

      /* close the writeable temp file, it isn't physically closed
       * because the descriptor has been duplicated
       */
      f->normal.flags &= ~PACKFILE_FLAG_CHUNK;
      pack_fclose(f);

      lseek(hndl, 0, SEEK_SET);

      /* create a readable pack file */
      tmp = _pack_fdopen(hndl, F_READ);
      if (!tmp)
         return NULL;

      _packfile_filesize = tmp->normal.todo - 4;

      header = pack_mgetl(tmp);

      pack_mputl(_packfile_filesize, parent);

      if (header == encrypt_id(F_PACK_MAGIC, TRUE))
         pack_mputl(-_packfile_datasize, parent);
      else
         pack_mputl(_packfile_datasize, parent);

      while ((c = pack_getc(tmp)) != EOF)
         pack_putc(c, parent);

      pack_fclose(tmp);

      unlink(name);
      free(name);
   }
   else
   {
      /* finish reading a chunk */
      while (f->normal.todo > 0)
         pack_getc(f);

      if (f->normal.unpack_data)
      {
         free_lzss_unpack_data(f->normal.unpack_data);
         f->normal.unpack_data = NULL;
      }

      if ((f->normal.passpos) && (f->normal.flags & PACKFILE_FLAG_OLD_CRYPT))
         parent->normal.passpos = parent->normal.passdata + (size_t)f->normal.passpos -
                                  (size_t)f->normal.passdata;

      free_packfile(f);
   }

   return parent;
}


/* pack_fseek:
 *  Like the stdio fseek() function, but only supports forward seeks
 *  relative to the current file position.
 */
int pack_fseek(PACKFILE *f, int offset)
{
   return f->vtable->pf_fseek(f->userdata, offset);
}


/* pack_getc:
 *  Returns the next character from the stream f, or EOF if the end of the
 *  file has been reached.
 */
int pack_getc(PACKFILE *f)
{
   return f->vtable->pf_getc(f->userdata);
}


/* pack_putc:
 *  Puts a character in the stream f.
 */
int pack_putc(int c, PACKFILE *f)
{
   return f->vtable->pf_putc(c, f->userdata);
}


/* pack_feof:
 *  pack_feof() returns nonzero as soon as you reach the end of the file. It
 *  does not wait for you to attempt to read beyond the end of the file,
 *  contrary to the ISO C feof() function.
 */
int pack_feof(PACKFILE *f)
{
   return f->vtable->pf_feof(f->userdata);
}


/* pack_ferror:
 *  Returns nonzero if the error indicator for the stream is set, indicating
 *  that an error has occurred during a previous operation on the stream.
 */
int pack_ferror(PACKFILE *f)
{
   return f->vtable->pf_ferror(f->userdata);
}


/* pack_igetw:
 *  Reads a 16 bit word from a file, using intel byte ordering.
 */
int pack_igetw(PACKFILE *f)
{
   int b1, b2;

   if ((b1 = pack_getc(f)) != EOF)
      if ((b2 = pack_getc(f)) != EOF)
         return ((b2 << 8) | b1);

   return EOF;
}


/* pack_igetl:
 *  Reads a 32 bit long from a file, using intel byte ordering.
 */
long pack_igetl(PACKFILE *f)
{
   int b1, b2, b3, b4;

   if ((b1 = pack_getc(f)) != EOF)
      if ((b2 = pack_getc(f)) != EOF)
         if ((b3 = pack_getc(f)) != EOF)
            if ((b4 = pack_getc(f)) != EOF)
               return (((long)b4 << 24) | ((long)b3 << 16) |
                       ((long)b2 << 8) | (long)b1);

   return EOF;
}


/* pack_iputw:
 *  Writes a 16 bit int to a file, using intel byte ordering.
 */
int pack_iputw(int w, PACKFILE *f)
{
   int b1, b2;

   b1 = (w & 0xFF00) >> 8;
   b2 = w & 0x00FF;

   if (pack_putc(b2, f) == b2)
      if (pack_putc(b1, f) == b1)
         return w;

   return EOF;
}


/* pack_iputl:
 *  Writes a 32 bit long to a file, using intel byte ordering.
 */
long pack_iputl(long l, PACKFILE *f)
{
   int b1, b2, b3, b4;

   b1 = (int)((l & 0xFF000000L) >> 24);
   b2 = (int)((l & 0x00FF0000L) >> 16);
   b3 = (int)((l & 0x0000FF00L) >> 8);
   b4 = (int)l & 0x00FF;

   if (pack_putc(b4, f) == b4)
      if (pack_putc(b3, f) == b3)
         if (pack_putc(b2, f) == b2)
            if (pack_putc(b1, f) == b1)
               return l;

   return EOF;
}


/* pack_mgetw:
 *  Reads a 16 bit int from a file, using motorola byte-ordering.
 */
int pack_mgetw(PACKFILE *f)
{
   int b1, b2;

   if ((b1 = pack_getc(f)) != EOF)
      if ((b2 = pack_getc(f)) != EOF)
         return ((b1 << 8) | b2);

   return EOF;
}


/* pack_mgetl:
 *  Reads a 32 bit long from a file, using motorola byte-ordering.
 */
long pack_mgetl(PACKFILE *f)
{
   int b1, b2, b3, b4;

   if ((b1 = pack_getc(f)) != EOF)
      if ((b2 = pack_getc(f)) != EOF)
         if ((b3 = pack_getc(f)) != EOF)
            if ((b4 = pack_getc(f)) != EOF)
               return (((long)b1 << 24) | ((long)b2 << 16) |
                       ((long)b3 << 8) | (long)b4);

   return EOF;
}


/* pack_mputw:
 *  Writes a 16 bit int to a file, using motorola byte-ordering.
 */
int pack_mputw(int w, PACKFILE *f)
{
   int b1, b2;

   b1 = (w & 0xFF00) >> 8;
   b2 = w & 0x00FF;

   if (pack_putc(b1, f) == b1)
      if (pack_putc(b2, f) == b2)
         return w;

   return EOF;
}


/* pack_mputl:
 *  Writes a 32 bit long to a file, using motorola byte-ordering.
 */
long pack_mputl(long l, PACKFILE *f)
{
   int b1, b2, b3, b4;

   b1 = (int)((l & 0xFF000000L) >> 24);
   b2 = (int)((l & 0x00FF0000L) >> 16);
   b3 = (int)((l & 0x0000FF00L) >> 8);
   b4 = (int)l & 0x00FF;

   if (pack_putc(b1, f) == b1)
      if (pack_putc(b2, f) == b2)
         if (pack_putc(b3, f) == b3)
            if (pack_putc(b4, f) == b4)
               return l;

   return EOF;
}


/* pack_fread:
 *  Reads n bytes from f and stores them at memory location p. Returns the
 *  number of items read, which will be less than n if EOF is reached or an
 *  error occurs. Error codes are stored in errno.
 */
long pack_fread(void *p, long n, PACKFILE *f)
{
   return f->vtable->pf_fread(p, n, f->userdata);
}


/* pack_fwrite:
 *  Writes n bytes to the file f from memory location p. Returns the number
 *  of items written, which will be less than n if an error occurs. Error
 *  codes are stored in errno.
 */
long pack_fwrite(const void *p, long n, PACKFILE *f)
{
   return f->vtable->pf_fwrite(p, n, f->userdata);
}


/* pack_ungetc:
 *  Puts a character back in the file's input buffer. It only works
 *  for characters just fetched by pack_getc and, like ungetc, only a
 *  single push back is guaranteed.
 */
int pack_ungetc(int c, PACKFILE *f)
{
   return f->vtable->pf_ungetc(c, f->userdata);
}


/* pack_fgets:
 *  Reads a line from a text file, storing it at location p. Stops when a
 *  linefeed is encountered, or max bytes have been read. Returns a pointer
 *  to where it stored the text, or NULL on error. The end of line is
 *  handled by detecting optional '\r' characters optionally followed
 *  by '\n' characters. This supports CR-LF (DOS/Windows), LF (Unix), and
 *  CR (Mac) formats.
 */
char *pack_fgets(char *p, int max, PACKFILE *f)
{
   char *pmax, *orig_p = p;
   int c;

   pack_error = 0;

   pmax = p + max - 1;

   if ((c = pack_getc(f)) == EOF)
   {
      if (max >= 1)
         *p = 0;
      return NULL;
   }

   do
   {

      if (c == '\r' || c == '\n')
      {
         /* Technically we should check there's space in the buffer, and if so,
          * add a \n.  But pack_fgets has never done this. */
         if (c == '\r')
         {
            /* eat the following \n, if any */
            c = pack_getc(f);
            if ((c != '\n') && (c != EOF))
               pack_ungetc(c, f);
         }
         break;
      }

      /* is there room in the buffer? */
      if ((pmax - p) < 1)
      {
         pack_ungetc(c, f);
         c = '\0';
         break;
      }

      /* write the character */
      *p = c;
      p++;
   }
   while ((c = pack_getc(f)) != EOF);

   /* terminate the string */
   *p = 0;

   if (c == '\0' || pack_error)
      return NULL;

   return orig_p; /* p has changed */
}


/* pack_fputs:
 *  Writes a string to a text file, returning zero on success, -1 on error.
 *  The input string is converted from the current text encoding format
 *  to UTF-8 before writing. Newline characters (\n) are written as \r\n
 *  on DOS and Windows platforms.
 */
int pack_fputs(const char *p, PACKFILE *f)
{
   const char *s = p;

   pack_error = 0;

   while (*s)
   {
      pack_putc(*s, f);
      s++;
   }

   if (pack_error)
      return -1;
   else
      return 0;
}


/* pack_get_userdata:
 *  Returns the userdata field of packfiles using user-defined vtables.
 */
void *pack_get_userdata(PACKFILE *f)
{
   return f->userdata;
}


/***************************************************
 ************ "Normal" packfile vtable *************
 ***************************************************
*/

static int normal_fclose(void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;
   int ret;

   if (f->normal.flags & PACKFILE_FLAG_WRITE)
   {
      if (f->normal.flags & PACKFILE_FLAG_CHUNK)
      {
         f = (PACKFILE *)pack_fclose_chunk((PACKFILE *)_f);
         if (!f)
            return -1;

         return pack_fclose(f);
      }

      normal_flush_buffer(f, TRUE);
   }

   if (f->normal.parent)
      ret = pack_fclose(f->normal.parent);
   else
   {
      ret = close(f->normal.hndl);
      if (ret != 0)
         pack_error = errno;
   }

   if (f->normal.pack_data)
   {
      free_lzss_pack_data(f->normal.pack_data);
      f->normal.pack_data = NULL;
   }

   if (f->normal.unpack_data)
   {
      free_lzss_unpack_data(f->normal.unpack_data);
      f->normal.unpack_data = NULL;
   }

   if (f->normal.passdata)
   {
      free(f->normal.passdata);
      f->normal.passdata = NULL;
      f->normal.passpos = NULL;
   }

   return ret;
}


/* normal_no_more_input:
 *  Return non-zero if the number of bytes remaining in the file (todo) is
 *  less than or equal to zero.
 *
 *  However, there is a special case.  If we are reading from a LZSS
 *  compressed file, the latest call to lzss_read() may have suspended while
 *  writing out a sequence of bytes due to the output buffer being too small.
 *  In that case the `todo' count would be decremented (possibly to zero),
 *  but the output isn't yet completely written out.
 */
static inline int normal_no_more_input(PACKFILE *f)
{
   /* see normal_refill_buffer() to see when lzss_read() is called */
   if (f->normal.parent && (f->normal.flags & PACKFILE_FLAG_PACK) &&
         _al_lzss_incomplete_state(f->normal.unpack_data))
      return 0;

   return (f->normal.todo <= 0);
}


static int normal_getc(void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;

   f->normal.buf_size--;
   if (f->normal.buf_size > 0)
      return *(f->normal.buf_pos++);

   if (f->normal.buf_size == 0)
   {
      if (normal_no_more_input(f))
         f->normal.flags |= PACKFILE_FLAG_EOF;
      return *(f->normal.buf_pos++);
   }

   return normal_refill_buffer(f);
}


static int normal_ungetc(int c, void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;

   if (f->normal.buf_pos == f->normal.buf)
      return EOF;
   else
   {
      *(--f->normal.buf_pos) = (unsigned char)c;
      f->normal.buf_size++;
      f->normal.flags &= ~PACKFILE_FLAG_EOF;
      return (unsigned char)c;
   }
}


static long normal_fread(void *p, long n, void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;
   unsigned char *cp = (unsigned char *)p;
   long i;
   int c;

   for (i = 0; i < n; i++)
   {
      if ((c = normal_getc(f)) == EOF)
         break;

      *(cp++) = c;
   }

   return i;
}


static int normal_putc(int c, void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;

   if (f->normal.buf_size + 1 >= F_BUF_SIZE)
   {
      if (normal_flush_buffer(f, FALSE))
         return EOF;
   }

   f->normal.buf_size++;
   return (*(f->normal.buf_pos++) = c);
}


static long normal_fwrite(const void *p, long n, void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;
   const unsigned char *cp = (const unsigned char *)p;
   long i;

   for (i = 0; i < n; i++)
   {
      if (normal_putc(*cp++, f) == EOF)
         break;
   }

   return i;
}


static int normal_fseek(void *_f, int offset)
{
   PACKFILE *f = (PACKFILE *)_f;
   int i;

   if (f->normal.flags & PACKFILE_FLAG_WRITE)
      return -1;

   pack_error = 0;

   /* skip forward through the buffer */
   if (f->normal.buf_size > 0)
   {
      i = MIN(offset, f->normal.buf_size);
      f->normal.buf_size -= i;
      f->normal.buf_pos += i;
      offset -= i;
      if ((f->normal.buf_size <= 0) && normal_no_more_input(f))
         f->normal.flags |= PACKFILE_FLAG_EOF;
   }

   /* need to seek some more? */
   if (offset > 0)
   {
      i = MIN(offset, f->normal.todo);

      if ((f->normal.flags & PACKFILE_FLAG_PACK) || (f->normal.passpos))
      {
         /* for compressed or encrypted files, we just have to read through the data */
         while (i > 0)
         {
            pack_getc(f);
            i--;
         }
      }
      else
      {
         if (f->normal.parent)
         {
            /* pass the seek request on to the parent file */
            pack_fseek(f->normal.parent, i);
         }
         else
         {
            /* do a real seek */
            lseek(f->normal.hndl, i, SEEK_CUR);
         }
         f->normal.todo -= i;
         if (normal_no_more_input(f))
            f->normal.flags |= PACKFILE_FLAG_EOF;
      }
   }

   if (pack_error)
      return -1;
   else
      return 0;
}


static int normal_feof(void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;

   return (f->normal.flags & PACKFILE_FLAG_EOF);
}



static int normal_ferror(void *_f)
{
   PACKFILE *f = (PACKFILE *)_f;

   return (f->normal.flags & PACKFILE_FLAG_ERROR);
}


/* normal_refill_buffer:
 *  Refills the read buffer. The file must have been opened in read mode,
 *  and the buffer must be empty.
 */
static int normal_refill_buffer(PACKFILE *f)
{
   int i, sz, done, offset;

   if (f->normal.flags & PACKFILE_FLAG_EOF)
      return EOF;

   if (normal_no_more_input(f))
   {
      f->normal.flags |= PACKFILE_FLAG_EOF;
      return EOF;
   }

   if (f->normal.parent)
   {
      if (f->normal.flags & PACKFILE_FLAG_PACK)
         f->normal.buf_size = lzss_read(f->normal.parent, f->normal.unpack_data,
                                        MIN(F_BUF_SIZE, f->normal.todo), f->normal.buf);
      else
         f->normal.buf_size = pack_fread(f->normal.buf, MIN(F_BUF_SIZE, f->normal.todo),
                                         f->normal.parent);
      if (f->normal.parent->normal.flags & PACKFILE_FLAG_EOF)
         f->normal.todo = 0;
      if (f->normal.parent->normal.flags & PACKFILE_FLAG_ERROR)
         goto Error;
   }
   else
   {
      f->normal.buf_size = MIN(F_BUF_SIZE, f->normal.todo);

      offset = lseek(f->normal.hndl, 0, SEEK_CUR);
      done = 0;

      errno = 0;
      sz = read(f->normal.hndl, f->normal.buf, f->normal.buf_size);

      while (sz + done < f->normal.buf_size)
      {
         if ((sz < 0) && ((errno != EINTR) && (errno != EAGAIN)))
            goto Error;

         if (sz > 0)
            done += sz;

         lseek(f->normal.hndl, offset + done, SEEK_SET);
         errno = 0;
         sz = read(f->normal.hndl, f->normal.buf + done, f->normal.buf_size - done);
      }

      if ((f->normal.passpos) && (!(f->normal.flags & PACKFILE_FLAG_OLD_CRYPT)))
      {
         for (i = 0; i < f->normal.buf_size; i++)
         {
            f->normal.buf[i] ^= *(f->normal.passpos++);
            if (!*f->normal.passpos)
               f->normal.passpos = f->normal.passdata;
         }
      }
   }

   f->normal.todo -= f->normal.buf_size;
   f->normal.buf_pos = f->normal.buf;
   f->normal.buf_size--;
   if (f->normal.buf_size <= 0)
      if (normal_no_more_input(f))
         f->normal.flags |= PACKFILE_FLAG_EOF;

   if (f->normal.buf_size < 0)
      return EOF;
   else
      return *(f->normal.buf_pos++);

Error:
   pack_error = TRUE;
   f->normal.flags |= PACKFILE_FLAG_ERROR;
   return EOF;
}


/* normal_flush_buffer:
 *  Flushes a file buffer to the disk. The file must be open in write mode.
 */
static int normal_flush_buffer(PACKFILE *f, int last)
{
   int i, sz, done, offset;

   if (f->normal.buf_size > 0)
   {
      if (f->normal.flags & PACKFILE_FLAG_PACK)
      {
         if (lzss_write(f->normal.parent, f->normal.pack_data, f->normal.buf_size,
                        f->normal.buf, last))
            goto Error;
      }
      else
      {
         if ((f->normal.passpos) && (!(f->normal.flags & PACKFILE_FLAG_OLD_CRYPT)))
         {
            for (i = 0; i < f->normal.buf_size; i++)
            {
               f->normal.buf[i] ^= *(f->normal.passpos++);
               if (!*f->normal.passpos)
                  f->normal.passpos = f->normal.passdata;
            }
         }

         offset = lseek(f->normal.hndl, 0, SEEK_CUR);
         done = 0;

         errno = 0;
         sz = write(f->normal.hndl, f->normal.buf, f->normal.buf_size);

         while (sz + done < f->normal.buf_size)
         {
            if ((sz < 0) && ((errno != EINTR) && (errno != EAGAIN)))
               goto Error;

            if (sz > 0)
               done += sz;

            lseek(f->normal.hndl, offset + done, SEEK_SET);
            errno = 0;
            sz = write(f->normal.hndl, f->normal.buf + done, f->normal.buf_size - done);
         }
      }
      f->normal.todo += f->normal.buf_size;
   }

   f->normal.buf_pos = f->normal.buf;
   f->normal.buf_size = 0;
   return 0;

Error:
   pack_error = TRUE;
   f->normal.flags |= PACKFILE_FLAG_ERROR;
   return EOF;
}
