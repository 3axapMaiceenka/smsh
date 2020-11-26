#include "shell.h"
#include "parser.h"
#include "hashtable.h"
#include "utility.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

/*
static void _test_(char* input)
{
	struct Shell* shell = start();

	initialize(shell, input);
	shell->program = parse(shell->parser);
	execute_print(shell, shell->program);

	stop(shell);
}*/

int main(int argc, char** argv)
{
	struct Shell* shell = start();

    while (1) 
    {
        char* input = readline("$ ");

        if (!input || !strcmp(input, "quit")) // temp
        {
        	free(input);
            break;
        }

        initialize(shell, input);
        shell->program = parse(shell->parser);
        execute(shell, shell->program);

        destroy_list(&shell->program);
        free(input);
    }

    stop(shell);
	
	return 0;
}