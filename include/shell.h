#ifndef SHELL_H
#define SHELL_H

#include "hashtable.h"
#include "parser.h"

#define DEFAULT_HASHTABLE_SIZE 128

struct Shell
{
	struct Parser* parser;
	struct Scanner* scanner;
	struct Hashtable* variables;
	struct Error* execution_error;
	CommandsList* program;
};

struct Shell* start();

void stop(struct Shell* shell);

void initialize(struct Shell* shell, char* buffer);

int set_variable(struct Shell* shell, const char* var_name, const char* var_value); // returns 1 if the variable already exists

int execute(struct Shell* shell, CommandsList* program);

void execute_print(struct Shell* shell, CommandsList* program);

#endif