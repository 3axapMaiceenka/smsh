#include "shell.h"
#include "utility.h"
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>

int main(int argc, char** argv)
{
	struct Shell* shell = start();
	int rc = 0;

	if (argc != 1)
	{
		rc = shell_execute_from_file(shell, argv[1]);
	}
	else
	{
		char* input = NULL;
		char* prompt = NULL;

		while (1) 
		{
			prompt = concat_strings(get_variable(shell, "PWD"), "$ ");
			input = readline(prompt);

			if (!input || !strcmp(input, "quit"))
			{
			  	break;
			}
		
			rc = shell_execute(shell, input);

			free(input);
			free(prompt);
		}

		free(input);
		free(prompt);
	}

	stop(shell);
	
	return rc;
}