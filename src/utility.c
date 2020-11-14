#include "utility.h"
#include <stdlib.h>
#include <string.h>

char* copy_string(const char* string)
{
	size_t str_len = strlen(string) + 1;
	char* str = malloc(str_len);

	memcpy(str, string, str_len);

	return str;
}