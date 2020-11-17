#ifndef SHELL_H
#define SHELL_H

#include "parser.h"
#include "hashtable.h"

#define DEFAULT_HASHTABLE_SIZE 128

struct Shell
{
	struct Parser* parser;
	struct Scanner* scanner;
	struct Hashtable* variables;
	struct Error* execution_error;
};

struct Shell* start();

void stop(struct Shell* shell);

void initialize(struct Shell* shell, char* buffer);

int set_variable(struct Shell* shell, const char* var_name, const char* var_value); // returns 1 if the variable already exists

void execute(struct Shell* shell, struct AstSimpleCommand* command);

#endif