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

void set_error(struct Error* error, const char* message)
{
	error->error_message = copy_string(message);
	error->error = 1;
}

void destroy_error(struct Error* error)
{
	if (error)
	{
		if (error->error_message)
		{
			free(error->error_message);
		}

		free(error);
	}
}