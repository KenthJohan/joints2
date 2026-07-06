#include "fs.h"
#include <stdio.h>
#include <stdlib.h>

char *fs_read_allocated(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}

	long size = ftell(file);
	if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	char *text = (char *)malloc((size_t)size + 1);
	if (text == NULL) {
		fclose(file);
		return NULL;
	}

	if (fread(text, 1, (size_t)size, file) != (size_t)size) {
		fclose(file);
		free(text);
		return NULL;
	}

	text[size] = '\0';
	fclose(file);
	return text;
}
