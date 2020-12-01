#include "shell.h"
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
		while (1) 
		{
			char* input = readline("$ ");
			if (!input || !strcmp(input, "quit"))
			{
				free(input);
			  	break;
			}
		
			rc = shell_execute(shell, input);
			free(input);
		}
	}

	stop(shell);
	
	return rc;
}