#include "shell.h"
#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Shell* start()
{
	struct Shell* shell = malloc(sizeof(struct Shell));
	shell->parser = calloc(1, sizeof(struct Parser));
	shell->scanner = calloc(1, sizeof(struct Scanner));
	shell->variables = create_hashtable(DEFAULT_HASHTABLE_SIZE);

	shell->parser->scanner = shell->scanner;
	shell->parser->error = calloc(1, sizeof(struct Error));

	return shell;
}

void stop(struct Shell* shell)
{
	if (shell)
	{
		if (shell->parser->error->error_message)
		{
			free(shell->parser->error->error_message);
		}

		free(shell->parser->error);
		free(shell->parser);
		free(shell->scanner);

		destroy_hashtable(shell->variables);
	}
}

void initialize(struct Shell* shell, char* buffer)
{
	shell->scanner->buffer = buffer;
	shell->scanner->position = 0;
	shell->parser->current_token = get_next_token(shell->scanner);
}

int set_variable(struct Shell* shell, const char* var_name, const char* var_value)
{
	struct Entry* var = get(shell->variables, var_name);

	if (!var)
	{
		insert(shell->variables, var_name, var_value);
		return 0;
	}
	else
	{
		free(var->value);
		var->value = copy_string(var_value);
		return 1;
	}
}

static const char* expand_token(struct Shell* shell, struct Token* token)
{
	switch (token->type)
	{
		case WORD:
		{
			return token->word.buffer;
		} break;
		case PARAMETER_EXPANSION:
		{
			struct Entry* entry = get(shell->variables, token->word.buffer);
			if (entry)
			{
				return entry->value;
			}
			else
			{
				return "";
			}
		} break;
		case NAME:
		{
			return token->word.buffer;
		} break;
		default: return "";
	}
}

void execute(struct Shell* shell, struct AstSimpleCommand* command, const char* buffer)
{
	if (shell->parser->error->error)
	{
		fprintf(stderr, "%s\n", shell->parser->error->error_message);
		free_ast_simple_command(command);

		return;
	}

	if (command)
	{
		if (command->assignment_list)
		{
			printf("Assignemtns:\n");

			for (struct Node* assignment = command->assignment_list->assignments->head; assignment; assignment = assignment->next)
			{
				struct AstAssignment* a = (struct AstAssignment*)assignment->data;

				printf("Variable name: %s\n", a->variable->word.buffer);

				struct AstNode* expr = a->expression;
				struct AstWord* word = NULL;

				switch (expr->node_type)
				{
					case AST_WORD:
					{
						word = expr->actual_data;

						switch (word->word.type)
						{
							case WORD:
							{
								set_variable(shell, a->variable->word.buffer, word->word.word.buffer);
								printf("Expression(WORD): %s\n", word->word.word.buffer);
							} break;
							case PARAMETER_EXPANSION:
							{
								struct Entry* entry = get(shell->variables, word->word.word.buffer);
								if (entry)
								{
									set_variable(shell, a->variable->word.buffer, entry->value);
									printf("Expression(PARAMETER_EXPANSION): %s -> %s\n", entry->key, entry->value);
								}
								else
								{
									set_variable(shell, a->variable->word.buffer, "");
									printf("Expression(PARAMETER_EXPANSION): %s -> 'empty'\n", word->word.word.buffer);
								}
							} break;
							default: break;
						}
					} break;

					default: printf("error\n"); break;
				}
			}
		}

		if (command->command_name)
		{
			printf("Command name: %s\n", expand_token(shell, &command->command_name->word));
		}

		if (command->command_args)
		{
			printf("Command arguments:\n");

			for (struct Node* argument = command->command_args->wordlist->head; argument; argument = argument->next)
			{
				struct AstWord* word = (struct AstWord*)(argument->data);
				printf("%s\n", expand_token(shell, &word->word));
			}
		}

		if (command->input_redirect)
		{
			printf("Input redirection to: %s\n", expand_token(shell, &command->input_redirect->file_name->word));
		}

		if (command->output_redirect)
		{
			printf("Output redirection to: %s\n", expand_token(shell, &command->output_redirect->file_name->word));
		}

		printf("\n");

		free_ast_simple_command(command);
	}
}