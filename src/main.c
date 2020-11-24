#include "shell.h"
#include "parser.h"
#include "hashtable.h"
#include "utility.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <readline/readline.h>
#include <readline/history.h>

static void _test_(char* input)
{
	struct Shell* shell = start();

	initialize(shell, input);
	shell->program = parse(shell->parser);
	execute_print(shell, shell->program);

	stop(shell);
}

int main(int argc, char** argv)
{
    while (1) 
    {
        char* input = readline("$ ");

        if (!input)
        {
            break;
        }

        _test_(input);

        free(input);
    }
	
	return 0;
}