#include "list.h"
#include "utility.h"
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

void unset_error(struct Error* error)
{
	error->error = 0;
	free(error->error_message);
	error->error_message = NULL;
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

size_t get_list_size(struct List* list)
{
	size_t size = 0;

	if (list)
	{
		for (struct Node* node = list->head; node; node = node->next)
		{
			size++;
		}
	}

	return size;
}

char* concat_strings(const char* str1, const char* str2)
{
	size_t len1 = strlen(str1);
	size_t len2 = strlen(str2);

	char* result = calloc(len1 + len2 + 1, sizeof(char));

	strncat(result, str1, len1);
	strncat(result, str2, len2);

	return result;
}