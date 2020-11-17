#ifndef UTILITY_H
#define UTILITY_H

struct Error
{
	char* error_message;
	int error; // 1 or 0
};

void set_error(struct Error* error, const char* message);

void destroy_error(struct Error* error);

char* copy_string(const char* string);

#endif