#ifndef _FILE_H_
#define _FILE_H_
#include <SDL2/SDL.h>

typedef struct {
	Uint64 size;
	char *buf;
} loaded_file;

loaded_file read_file(const char *file_name);
void loaded_file_destroy(loaded_file *self);

#endif // !_FILE_H_
