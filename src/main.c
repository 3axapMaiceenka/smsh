#include "shell.h"
#include "job.h"
#include "utility.h"
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>

int main(int argc, char** argv)
{
	struct Shell* shell = create();

	if (!shell_init(shell))
	{
		return 1;
	}

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

			if (!strcmp(input, "quit"))
			{
			  	break;
			}
		
			rc = shell_execute(shell, input);

			free(input);
			free(prompt);

			do_job_notification(shell->jobs);
		}

		free(input);
		free(prompt);
	}

	destroy(shell);
	
	return rc;
}