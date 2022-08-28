#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "alport.h"


static void *load_file_object(PACKFILE *f, long size);
void *load_dat_palette(PACKFILE *f, long size);
void *load_sample_object(PACKFILE *f, long size);
void unload_sample(SAMPLE *s);


/* list of objects, and methods for loading and destroying them */
DATAFILE_TYPE _datafile_type[MAX_DATAFILE_TYPES] =
{
   { DAT_FILE, load_file_object, (void (*)(void *data))unload_datafile },
   { DAT_PALETTE, load_dat_palette, NULL },
   { DAT_SAMPLE, load_sample_object, (void (*)(void *data))unload_sample },
   { DAT_MIDI, load_midi_object, NULL },
   { DAT_FONT, load_dat_font, (void (*)(void *data))destroy_font },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL },
   { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }, { DAT_END, NULL, NULL }
};


static void (*datafile_callback)(DATAFILE *) = NULL;


/* register_datafile_object:
 *  Registers a custom datafile object, providing functions for loading
 *  and destroying the objects.
 */
void register_datafile_object(int id, void *(*load)(PACKFILE *f, long size),
                              void (*destroy)(void *data))
{
   int i;

   /* replacing an existing type? */
   for (i = 0; i < MAX_DATAFILE_TYPES; i++)
   {
      if (_datafile_type[i].type == id)
      {
         if (load)
            _datafile_type[i].load = load;
         if (destroy)
            _datafile_type[i].destroy = destroy;
         return;
      }
   }

   /* add a new type */
   for (i = 0; i < MAX_DATAFILE_TYPES; i++)
   {
      if (_datafile_type[i].type == DAT_END)
      {
         _datafile_type[i].type = id;
         _datafile_type[i].load = load;
         _datafile_type[i].destroy = destroy;
         return;
      }
   }
}


/* read_block:
 *  Reads a block of size bytes from a file, allocating memory to store it.
 */
void *read_block(PACKFILE *f, int size, int alloc_size)
{
   void *p;

   p = malloc(MAX(size, alloc_size));
   if (!p)
      return NULL;

   pack_fread(p, size, f);

   if (pack_ferror(f))
   {
      free(p);
      return NULL;
   }

   return p;
}


/* load_data_object:
 *  Loads a binary data object from a datafile.
 */
static void *load_data_object(PACKFILE *f, long size)
{
   return read_block(f, size, 0);
}


/* load_object:
 *  Helper to load an object from a datafile and store it in 'obj'.
 *  Returns 0 on success and -1 on failure.
 */
static int load_object(DATAFILE *obj, PACKFILE *f, int type)
{
   PACKFILE *ff;
   int d, i;

   /* load actual data */
   ff = pack_fopen_chunk(f, FALSE);

   if (ff)
   {
      d = ff->normal.todo;

      /* look for a load function */
      for (i = 0; i < MAX_DATAFILE_TYPES; i++)
      {
         if (_datafile_type[i].type == type)
         {
            obj->dat = _datafile_type[i].load(ff, d);
            goto Found;
         }
      }

      /* if not found, load binary data */
      obj->dat = load_data_object(ff, d);

Found:
      pack_fclose_chunk(ff);

      if (!obj->dat)
         return -1;

      obj->type = type;
      obj->size = d;
      return 0;
   }

   return -1;
}


/* _load_property:
 *  Helper to load a property from a datafile and store it in 'prop'.
 *  Returns 0 on success and -1 on failure.
 */
int _load_property(DATAFILE_PROPERTY *prop, PACKFILE *f)
{
   int type, size;

   type = pack_mgetl(f);
   size = pack_mgetl(f);

   prop->type = type;
   prop->dat = (char *)malloc(size + 1); /* '1' for end-of-string delimiter */
   if (!prop->dat)
   {
      pack_fseek(f, size);
      return -1;
   }

   /* read the property */
   pack_fread(prop->dat, size, f);
   prop->dat[size] = 0;

   return 0;
}


/* _add_property:
 *  Helper to add a new property to a property list. Returns 0 on
 *  success or -1 on failure.
 */
int _add_property(DATAFILE_PROPERTY **list, DATAFILE_PROPERTY *prop)
{
   DATAFILE_PROPERTY *iter;
   int length = 0;

   /* find the length of the list */
   if (*list)
   {
      iter = *list;
      while (iter->type != DAT_END)
      {
         length++;
         iter++;
      }
   }

   /* grow the list */
   *list = (DATAFILE_PROPERTY *)realloc(*list,
                                        sizeof(DATAFILE_PROPERTY) * (length + 2));
   if (!(*list))
      return -1;

   /* copy the new property */
   (*list)[length] = *prop;

   /* set end-of-array marker */
   (*list)[length + 1].type = DAT_END;
   (*list)[length + 1].dat = NULL;

   return 0;
}


/* _destroy_property_list:
 *  Helper to destroy a property list.
 */
void _destroy_property_list(DATAFILE_PROPERTY *list)
{
   int c;

   for (c = 0; list[c].type != DAT_END; c++)
   {
      if (list[c].dat)
         free(list[c].dat);
   }

   free(list);
}


/* load_file_object:
 *  Loads a datafile object.
 */
static void *load_file_object(PACKFILE *f, long size)
{
   DATAFILE *dat;
   DATAFILE_PROPERTY prop, *list;
   int count, c, type, failed;

   (void)size;

   count = pack_mgetl(f);

   dat = (DATAFILE *)malloc(sizeof(DATAFILE) * (count + 1));
   if (!dat)
      return NULL;

   list = NULL;
   failed = FALSE;

   /* search the packfile for properties or objects */
   for (c = 0; c < count;)
   {
      type = pack_mgetl(f);

      if (type == DAT_PROPERTY)
      {
         if ((_load_property(&prop, f) != 0) || (_add_property(&list, &prop) != 0))
         {
            failed = TRUE;
            break;
         }
      }
      else
      {
         if (load_object(&dat[c], f, type) != 0)
         {
            failed = TRUE;
            break;
         }

         /* attach the property list to the object */
         dat[c].prop = list;
         list = NULL;

         if (datafile_callback)
            datafile_callback(dat + c);

         c++;
      }
   }

   /* set end-of-array marker */
   dat[c].type = DAT_END;
   dat[c].dat = NULL;

   /* destroy the property list if not assigned to an object */
   if (list)
      _destroy_property_list(list);

   /* gracefully handle failure */
   if (failed)
   {
      unload_datafile(dat);
      dat = NULL;
   }

   return dat;
}


/* load_datafile:
 *  Loads an entire data file into memory, and returns a pointer to it.
 *  On error, sets errno and returns NULL.
 */
DATAFILE *load_datafile(const char *filename)
{
   return load_datafile_callback(filename, NULL);
}


/* load_datafile_callback:
 *  Loads an entire data file into memory, and returns a pointer to it.
 *  On error, sets errno and returns NULL.
 */
DATAFILE *load_datafile_callback(const char *filename,
                                 void (*callback)(DATAFILE *))
{
   PACKFILE *f;
   DATAFILE *dat;
   int type;

   f = pack_fopen(filename, F_READ_PACKED);
   if (!f)
      return NULL;

   if ((f->normal.flags & PACKFILE_FLAG_CHUNK)
         && (!(f->normal.flags & PACKFILE_FLAG_EXEDAT)))
      type = (_packfile_type == DAT_FILE) ? DAT_MAGIC : 0;
   else
      type = pack_mgetl(f);

   if (type == DAT_MAGIC)
   {
      datafile_callback = callback;
      dat = (DATAFILE *)load_file_object(f, 0);
      datafile_callback = NULL;
   }
   else
      dat = NULL;

   pack_fclose(f);
   return dat;
}


/* create_datafile_index
 *  Reads offsets of all objects inside datafile.
 *  On error, sets errno and returns NULL.
 */
DATAFILE_INDEX *create_datafile_index(const char *filename)
{
   PACKFILE *f;
   DATAFILE_INDEX *index;
   long pos = 4;
   int type, count, skip, i;

   f = pack_fopen(filename, F_READ_PACKED);
   if (!f)
      return NULL;

   if ((f->normal.flags & PACKFILE_FLAG_CHUNK)
         && (!(f->normal.flags & PACKFILE_FLAG_EXEDAT)))
      type = (_packfile_type == DAT_FILE) ? DAT_MAGIC : 0;
   else
   {
      type = pack_mgetl(f);
      pos += 4;
   }

   /* only support V2 datafile format */
   if (type != DAT_MAGIC)
      return NULL;

   count = pack_mgetl(f);
   pos += 4;

   index = (DATAFILE_INDEX *)malloc(sizeof(DATAFILE_INDEX));
   if (!index)
   {
      pack_fclose(f);
      return NULL;
   }

   index->filename = strdup(filename);
   if (!index->filename)
   {
      pack_fclose(f);
      free(index);
      return NULL;
   }

   index->offset = (long *)malloc(sizeof(long) * count);
   if (!index->offset)
   {
      pack_fclose(f);
      free(index->filename);
      free(index);
      return NULL;
   }

   for (i = 0; i < count; ++i)
   {
      index->offset[i] = pos;

      /* Skip properties */
      while (pos += 4, pack_mgetl(f) == DAT_PROPERTY)
      {

         /* Skip property name */
         pack_fseek(f, 4);
         pos += 4;

         /* Skip rest of property */
         skip = pack_mgetl(f);
         pos += 4;      /* Get property size */
         pack_fseek(f, skip);
         pos += skip;
      }

      /* Skip rest of object */
      skip = pack_mgetl(f) + 4;
      pos += 4;
      pack_fseek(f, skip);
      pos += skip;

   }
   pack_fclose(f);
   return index;
}


/* load_datafile_object:
 *  Loads a single object from a datafile.
 */
DATAFILE *load_datafile_object(const char *filename, const char *objectname)
{
   PACKFILE *f;
   DATAFILE *dat;
   DATAFILE_PROPERTY prop, *list;
   char parent[1024], child[1024];
   char *bufptr, *prevptr, *separator;
   int count, c, type, size, found;

   /* concatenate to filename#objectname */
   strcpy(parent, filename);

   if (strcmp(parent, "#") != 0)
      strcat(parent, "#");

   strcat(parent, objectname);


   /* split into path and actual objectname (for nested files) */
   prevptr = bufptr = parent;
   separator = NULL;
   while ((c = *bufptr) != 0)
   {
      if ((c == '#') || (c == '/') || (c == OTHER_PATH_SEPARATOR))
         separator = prevptr;

      prevptr = bufptr++;
   }

   strcpy(child, separator + 1);

   if (separator == parent)
      *(separator + 1) = 0;
   else
      *separator = 0;

   /* open the parent datafile */
   f = pack_fopen(parent, F_READ_PACKED);
   if (!f)
      return NULL;

   if ((f->normal.flags & PACKFILE_FLAG_CHUNK)
         && (!(f->normal.flags & PACKFILE_FLAG_EXEDAT)))
      type = (_packfile_type == DAT_FILE) ? DAT_MAGIC : 0;
   else
      type = pack_mgetl(f);

   /* only support V2 datafile format */
   if (type != DAT_MAGIC)
   {
      pack_fclose(f);
      return NULL;
   }

   count = pack_mgetl(f);

   dat = NULL;
   list = NULL;
   found = FALSE;

   /* search the packfile for properties or objects */
   for (c = 0; c < count;)
   {
      type = pack_mgetl(f);

      if (type == DAT_PROPERTY)
      {
         if ((_load_property(&prop, f) != 0) || (_add_property(&list, &prop) != 0))
            break;

         if ((prop.type == DAT_NAME) && (strcasecmp(prop.dat, child) == 0))
            found = TRUE;
      }
      else
      {
         if (found)
         {
            /* we have found the object, load it */
            dat = (DATAFILE *)malloc(sizeof(DATAFILE));
            if (!dat)
               break;

            if (load_object(dat, f, type) != 0)
            {
               free(dat);
               dat = NULL;
               break;
            }

            /* attach the property list to the object */
            dat->prop = list;
            list = NULL;

            break;
         }
         else
         {
            /* skip an unwanted object */
            size = pack_mgetl(f);
            pack_fseek(f, size + 4); /* '4' for the chunk size */

            /* destroy the property list */
            if (list)
            {
               _destroy_property_list(list);
               list = NULL;
            }

            c++;
         }
      }
   }

   /* destroy the property list if not assigned to an object */
   if (list)
      _destroy_property_list(list);

   pack_fclose(f);
   return dat;
}


/* load_datafile_object_indexed
 *  Loads a single object from a datafile using its offset.
 *  On error, returns NULL.
 */
DATAFILE *load_datafile_object_indexed(const DATAFILE_INDEX *index, int item)
{
   int type;
   PACKFILE *f;
   DATAFILE *dat;
   DATAFILE_PROPERTY prop, *list = NULL;

   f = pack_fopen(index->filename, F_READ_PACKED);
   if (!f)
      return NULL;

   dat = (DATAFILE *)malloc(sizeof(DATAFILE));
   if (!dat)
   {
      pack_fclose(f);
      return NULL;
   }

   /* pack_fopen will read first 4 bytes for us */
   pack_fseek(f, index->offset[item] - 4);

   do
      type = pack_mgetl(f);
   while (type == DAT_PROPERTY && _load_property(&prop, f)  == 0 &&
          _add_property(&list, &prop) == 0);


   if (load_object(dat, f, type) != 0)
   {
      pack_fclose(f);
      free(dat);
      _destroy_property_list(list);
      return NULL;
   }

   /* attach the property list to the object */
   dat->prop = list;

   pack_fclose(f);
   return dat;
}


/* _unload_datafile_object:
 *  Helper to destroy a datafile object.
 */
void _unload_datafile_object(DATAFILE *dat)
{
   int i;

   /* destroy the property list */
   if (dat->prop)
      _destroy_property_list(dat->prop);

   /* look for a destructor function */
   for (i = 0; i < MAX_DATAFILE_TYPES; i++)
   {
      if (_datafile_type[i].type == dat->type)
      {
         if (dat->dat)
         {
            if (_datafile_type[i].destroy)
               _datafile_type[i].destroy(dat->dat);
            else
               free(dat->dat);
         }
         return;
      }
   }

   /* if not found, just free the data */
   if (dat->dat)
      free(dat->dat);
}


/* unload_datafile:
 *  Frees all the objects in a datafile.
 */
void unload_datafile(DATAFILE *dat)
{
   int i;

   if (dat)
   {
      for (i = 0; dat[i].type != DAT_END; i++)
         _unload_datafile_object(dat + i);

      free(dat);
   }
}


/* unload_datafile_index
 *  Frees memory used by datafile index
 */
void destroy_datafile_index(DATAFILE_INDEX *index)
{
   if (index)
   {
      free(index->filename);
      free(index->offset);
      free(index);
   }
}


/* unload_datafile_object:
 *  Unloads a single datafile object, returned by load_datafile_object().
 */
void unload_datafile_object(DATAFILE *dat)
{
   if (dat)
   {
      _unload_datafile_object(dat);
      free(dat);
   }
}


/* find_datafile_object:
 *  Returns a pointer to the datafile object with the given name
 */
DATAFILE *find_datafile_object(const DATAFILE *dat, const char *objectname)
{
   char name[512];
   int recurse = FALSE;
   int pos, c;

   /* split up the object name */
   pos = 0;

   while ((c = objectname[pos]) != 0)
   {
      if ((c == '#') || (c == '/') || (c == OTHER_PATH_SEPARATOR))
      {
         recurse = TRUE;
         break;
      }
      name[pos++] = c;
   }

   name[pos] = 0;

   /* search for the requested object */
   for (pos = 0; dat[pos].type != DAT_END; pos++)
   {
      if (strcasecmp(name, get_datafile_property(dat + pos, DAT_NAME)) == 0)
      {
         if (recurse)
         {
            if (dat[pos].type == DAT_FILE)
               return find_datafile_object((const DATAFILE *)dat[pos].dat, objectname);
            else
               return NULL;
         }
         else
            return (DATAFILE *)dat + pos;
      }
   }

   /* oh dear, the object isn't there... */
   return NULL;
}


/* get_datafile_property:
 *  Returns the specified property string for the datafile object, or
 *  an empty string if the property does not exist.
 */
const char *get_datafile_property(const DATAFILE *dat, int type)
{
   DATAFILE_PROPERTY *prop;

   prop = dat->prop;
   if (prop)
   {
      while (prop->type != DAT_END)
      {
         if (prop->type == type)
            return (prop->dat) ? prop->dat : NULL;

         prop++;
      }
   }

   return NULL;
}
