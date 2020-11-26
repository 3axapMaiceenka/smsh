#ifndef UTILITY_H
#define UTILITY_H

#include <stdlib.h>

struct Error
{
	char* error_message;
	int error; // 1 or 0
};

void set_error(struct Error* error, const char* message);

void unset_error(struct Error* error);

void destroy_error(struct Error* error);

char* copy_string(const char* string);

char* concat_strings(const char* str1, const char* str2);

size_t get_list_size(struct List* list);

#endif