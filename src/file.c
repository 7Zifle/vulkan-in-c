#include "file.h"

#include <stdlib.h>
#include <stdio.h>

loaded_file read_file(const char *file_name)
{
	FILE *file = fopen(file_name, "rb");

	if (file == NULL) {
		fprintf(stderr, "Error opening file: %s\n", file_name);
	}

	fseek(file, 0, SEEK_END); 
	Uint32 file_size = ftell(file);
	rewind(file);

	char *code = aligned_alloc(sizeof(Uint32), sizeof(char) * file_size);
	if (code == NULL) {
		perror("aligned_alloc");
		fclose(file);
	}

	fread(code, file_size, 1, file);

	fclose(file);

	loaded_file lf = {
		.buf = code,
		.size = file_size,
	};

	return lf;
}

void loaded_file_destroy(loaded_file *self)
{
	free(self->buf);
}
