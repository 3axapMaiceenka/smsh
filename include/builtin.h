#ifndef BUILTIN_H
#define BUILTIN_H

#include "shell.h"

struct Builtin
{
	const char* name;
	int (*exec)(struct Shell* shell, char** args); // last *args is NULL
}; 

const struct Builtin* const is_builtin(const char* name);

#endif