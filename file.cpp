#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "alport.h"


/* delete_file:
 *  Removes a file from the disk. You can't delete directories.
 *  Returns zero on success, non-zero on failure. 
 */
int delete_file(const char *filename)
{
   if (unlink(filename) != 0)
      return -1;

   return 0;
}


/* file_exists:
 *  Checks whether a file matching the given name exists,
 *  returning non zero if it does.
 */
int file_exists(const char *filename)
{
   FILE *fp;

   fp = fopen(filename, "rb");
   if (!fp)
      return FALSE;

   fclose(fp);

   return TRUE;
}


/* file_size:
 *  Returns the size of a file, in bytes.
 *  If the file does not exist or an error occurs, it will return zero.
 *  Only works on not opened files.
 */
size_t file_size(const char *filename)
{
   FILE *fp;
   size_t size;

   fp = fopen(filename, "rb");
   if (!fp)
      return 0;

   fseek(fp, 0, SEEK_END);
   size = ftell(fp);

   fclose(fp);

   return size;
}


/* get_filename:
 *  When passed a completely specified file path, this returns a pointer
 *  to the filename portion. Both '\' and '/' are recognized as directory
 *  separators.
 */
char *get_filename(const char *path)
{
   int c;
   const char *ptr, *ret;

   ptr = path;
   ret = ptr;
   for (;;)
   {
      c = *ptr++;
      if (!c)
         break;

      if ((c == '/') || (c == OTHER_PATH_SEPARATOR) || (c == DEVICE_SEPARATOR))
         ret = (char *)ptr;
   }

   return (char *)ret;
}


/* get_extension:
 *  When passed a complete filename (with or without path information)
 *  this returns a pointer to the file extension.
 */
char *get_extension(const char *filename)
{
   int pos, c;

   pos = strlen(filename);

   while (pos > 0)
   {
      c = filename[pos - 1];
      if ((c == '.') || (c == '/') || (c == OTHER_PATH_SEPARATOR)
            || (c == DEVICE_SEPARATOR))
         break;
      pos--;
   }

   if ((pos > 0) && (filename[pos - 1] == '.'))
      return (char *)filename + pos;

   return (char *)filename + strlen(filename);
}


/* put_backslash:
 *  If the last character of the filename is not a \, /, or #, or a device
 *  separator (eg. : under DOS), this routine will concatenate a \ or / on
 *  to it (depending on platform).
 */
void put_backslash(char *filename)
{
   int c;

   if (*filename)
   {
      c = *(filename + strlen(filename) - 1);
      if ((c == '/') || (c == OTHER_PATH_SEPARATOR) || (c == DEVICE_SEPARATOR)
            || (c == '#'))
         return;
   }

   filename += strlen(filename);
   *(filename++) = OTHER_PATH_SEPARATOR;
   *filename = '\0';
}


/* replace_filename:
 *  Replaces path+filename in path with different one.
 *  You can use the same buffer both as input and output.
 *  It does not append '/' to the path.
 */
char *replace_filename(char *dest, const char *path, const char *filename)
{
   char tmp[1024] = {'\0'};
   int pos, c;

   pos = strlen(path);

   while (pos > 0)
   {
      c = path[pos - 1];
      if ((c == '/') || (c == OTHER_PATH_SEPARATOR) || (c == DEVICE_SEPARATOR))
         break;
      pos--;
   }

   strncpy(tmp, path, pos);
   strcat(tmp, filename);

   strcpy(dest, tmp);

   return dest;
}


/* replace_extension:
 *  Replaces extension in filename with different one.
 *  It appends '.' if it is not present in the filename.
 *  You can use the same buffer both as input and output.
 */
char *replace_extension(char *dest, const char *filename, const char *ext)
{
   char tmp[1024] = {'\0'};
   int pos, end, c;

   pos = end = strlen(filename);

   while (pos > 0)
   {
      c = filename[pos - 1];
      if ((c == '.') || (c == '/') || (c == OTHER_PATH_SEPARATOR)
            || (c == DEVICE_SEPARATOR))
         break;
      pos--;
   }

   if (filename[pos - 1] == '.')
      end = pos - 1;

   strncpy(tmp, filename, end);
   strcat(tmp, ".");
   strcat(tmp, ext);

   strcpy(dest, tmp);

   return dest;
}
