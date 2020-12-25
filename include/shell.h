#ifndef SHELL_H
#define SHELL_H

#include "hashtable.h"
#include "parser.h"
#include "job.h"

struct Shell
{
	struct Parser* parser;
	struct Scanner* scanner;
	struct Hashtable* variables;
	struct Error* execution_error;
	CommandsList* program;
	Jobs* jobs;
	pid_t pgid;
};

struct Shell* create();
int shell_init(struct Shell* shell);
void destroy(struct Shell* shell);
void init_parser(struct Shell* shell, char* buffer);
int set_variable(struct Shell* shell, const char* var_name, const char* var_value); // returns 1 if the variable already exists
int shell_execute(struct Shell* shell, char* buffer);
int shell_execute_from_file(struct Shell* shell, const char* filename);
int execute(struct Shell* shell, CommandsList* program);
void execute_print(struct Shell* shell, CommandsList* program);
const char* get_variable(struct Shell* shell, const char* var_name);

#endif