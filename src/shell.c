#include "shell.h"
#include "utility.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char** environ;

static void init_from_env(struct Shell* shell)
{
	size_t cap = 32;
	size_t size = 0;
	char* buf = malloc(sizeof(char) * cap);

	for (char** str = environ; *str; str++)
	{
		const char* value = strchr(*str, '=');
		size = value - *str + 1;

		if (size > cap)
		{
			cap = size;
			buf = realloc(buf, cap * sizeof(char));
		}

		strncpy(buf, *str, size);
		buf[size - 1] = '\0';

		insert(shell->variables, buf, value + 1);
	}

	free(buf);
}

struct Shell* start()
{
	struct Shell* shell = calloc(1, sizeof(struct Shell));
	shell->parser = calloc(1, sizeof(struct Parser));
	shell->scanner = calloc(1, sizeof(struct Scanner));
	shell->execution_error = calloc(1, sizeof(struct Error));
	shell->variables = create_hashtable(DEFAULT_HASHTABLE_SIZE);

	shell->parser->scanner = shell->scanner;
	shell->parser->error = calloc(1, sizeof(struct Error));

	init_from_env(shell);

	return shell;
}

void stop(struct Shell* shell)
{
	if (shell)
	{
		destroy_error(shell->parser->error);
		destroy_error(shell->execution_error);

		free(shell->parser);
		free(shell->scanner);

		destroy_list(&shell->program);
		destroy_hashtable(&shell->variables);
	}
}

void initialize(struct Shell* shell, char* buffer)
{
	shell->scanner->buffer = buffer;
	shell->scanner->position = 0;
	shell->parser->parsing_arithm_expr = 0;
	shell->parser->current_token = get_next_token(shell->scanner, &shell->parser->parsing_arithm_expr);
}

int set_variable(struct Shell* shell, const char* var_name, const char* var_value)
{
	char** value = get(shell->variables, var_name);

	if (!value)
	{
		insert(shell->variables, var_name, var_value);
		return 0;
	}
	else
	{
		size_t size = strlen(var_value) + 1;
		*value = realloc(*value, size);
		memcpy(*value, var_value, size);
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
			char** value = get(shell->variables, token->word.buffer);
			if (value)
			{
				return *value;
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

static int is_integer(const char* string)
{
	if (*string == '-' || (*string >= '0' && *string <= '9'))
	{
		string++;

		for (; *string; string++)
		{
			if (*string < '0' || *string > '9')
			{
				return 0;
			}
		}

		return 1;
	}

	return 0;
}

int execute_arithm_expr(struct Shell* shell, struct AstArithmExpr* arithm_expr)
{
	switch (arithm_expr->token.type)
	{
		case INTEGER:
		{
			return atoi(arithm_expr->token.word.buffer);
		} break;
		case PARAMETER_EXPANSION:
		{
			const char* value = expand_token(shell, &arithm_expr->token);

			if (is_integer(value))
			{
				return atoi(value);
			}
			else
			{
				if (!shell->execution_error->error)
				{
					set_error(shell->execution_error, "invalid parameter expansion inside arithmetic expansion!");
				}
			}
		} break;
		case MULTIPLY:
		{
			return execute_arithm_expr(shell, arithm_expr->left) * execute_arithm_expr(shell, arithm_expr->right);
		} break;
		case DIVIDE:
		{
			return execute_arithm_expr(shell, arithm_expr->left) / execute_arithm_expr(shell, arithm_expr->right);
		} break;
		case PLUS:
		{
			if (arithm_expr->right)
			{
				return execute_arithm_expr(shell, arithm_expr->left) + execute_arithm_expr(shell, arithm_expr->right);
			}
			else
			{
				return execute_arithm_expr(shell ,arithm_expr->left);
			}
		} break;
		case MINUS:
		{
			if (arithm_expr->right)
			{
				return execute_arithm_expr(shell, arithm_expr->left) - execute_arithm_expr(shell, arithm_expr->right);
			}
			else
			{
				return -execute_arithm_expr(shell, arithm_expr->left);
			}
		} break;
		default: break;
	}

	return 0;
}

static void exec_compound_cmd_list(struct Shell* shell, CompoundCommandsList* cmd_list)
{
	printf("Compount commands execution is not implemented yet\n");
}

static void exec_assignment(struct Shell* shell, struct AstAssignment* assignment)
{
	if (assignment->expression->node_type == AST_WORD)
	{
		struct AstWord* word = (struct AstWord*)assignment->expression->actual_data;
		set_variable(shell, expand_token(shell, assignment->variable), expand_token(shell, &word->word));
	}
	else // assignment->node_type == AST_ARITHM_EXPR
	{
		char buffer[11]; // int is maximum 11 digits long
		sprintf(buffer, "%d", execute_arithm_expr(shell, (struct AstArithmExpr*)assignment->expression->actual_data));

		if (!shell->execution_error->error)
		{
			set_variable(shell, expand_token(shell, assignment->variable), buffer);
		}
	}
}

static void exec_assignments_list(struct Shell* shell, AssignmentsList* assignments_list)
{
	for (struct Node* node = assignments_list->head; node; node = node->next)
	{
		exec_assignment(shell, (struct AstAssignment*)node->data);

		if (shell->execution_error->error)
		{
			return;
		}
	}
}

// TODO: add i/o redirection
static int start_process(const char* cmd_name, char** cmd_args, enum RunningMode mode)
{
	pid_t pid = fork();

	switch (pid)
	{
		case -1:
		{
			return -1;
		} 
		case 0:
		{
			if (execvp(cmd_name, cmd_args) == -1)
			{
				return -1;
			}
		} break;
		default:
		{
			if (mode == FOREGROUND)
			{
				int status;
				do
				{
					waitpid(pid, &status, WUNTRACED); // wait while child process is running
				} while (!WIFEXITED(status) && !WIFSIGNALED(status)); // resume if child process has terminated
			}
		} break;
	}

	return 0;
}

static void free_cmd_args(char** argv)
{
	for (char** ptr = argv; *ptr; ptr++)
	{
		free(*ptr);
	}

	free(argv);
}

static char** create_cmd_args(struct Shell* shell, const char* cmd_name, Wordlist* command_args)
{
	size_t argc = get_list_size(command_args) + 2;
	char** argv = calloc(argc, sizeof(char*));
	size_t indx = 0;

	argv[0] = copy_string(cmd_name);

	if (command_args)
	{
		for (struct Node* node = command_args->head; node; node = node->next)
		{
			struct AstNode* ast_node = (struct AstNode*)node->data;

			if (ast_node->node_type == AST_WORD)
			{
				struct AstWord* ast_word = (struct AstWord*)ast_node->actual_data;
				argv[++indx] = copy_string(expand_token(shell, &ast_word->word));
			}
			else
			{
				struct AstArithmExpr* arithm_expr = (struct AstArithmExpr*)ast_node->actual_data;

				char buffer[11]; // int is maximum 11 digits long
				sprintf(buffer, "%d", execute_arithm_expr(shell, arithm_expr));
		
				if (shell->execution_error->error)
				{
					free_cmd_args(argv);
					return NULL;
				}

				argv[++indx] = copy_string(buffer);
			}
		}
	}

	return argv;
}

// For now just calls fork() and execvp()
static void exec_simple_command(struct Shell* shell, struct AstSimpleCommand* simple_command, enum RunningMode mode)
{
	// TODO: add i/o redirection, builtin commands
	
	if (simple_command->assignment_list)
	{
		exec_assignments_list(shell, simple_command->assignment_list);
	}

	if (shell->execution_error->error)
	{
		return;
	}

	if (simple_command->command_name)
	{
		const char* cmd_name = expand_token(shell, &simple_command->command_name->word);
		char** cmd_args = create_cmd_args(shell, cmd_name, simple_command->command_args);

		if (shell->execution_error->error)
		{
			return;
		}

		if (start_process(cmd_name, cmd_args, mode) == -1)
		{
			char* error_mes = concat_strings(cmd_name, " - command not found");
			set_error(shell->execution_error, error_mes);
			free(error_mes);
		}

		free_cmd_args(cmd_args);
	}
}

static void exec_pipeline(struct Shell* shell, struct AstPipeline* pipe)
{
	if (pipe)
	{
		for (struct Node* node = pipe->pipeline->head; node; node = node->next)
		{
			// TODO: add piping. No i/o redirection for now
			exec_simple_command(shell, (struct AstSimpleCommand*)node->data, pipe->mode);

			if (shell->execution_error->error)
			{
				return;
			}
		}
	}
}

static void exec_pipeline_list(struct Shell* shell, PipelinesList* pipe_list)
{
	if (pipe_list)
	{
		for (struct Node* node = pipe_list->head; node; node = node->next)
		{
			exec_pipeline(shell, (struct AstPipeline*)node->data);

			if (shell->execution_error->error)
			{
				return;
			}
		}
	}
}

static int handle_error(struct Error* error, const char* prompt)
{
	if (error->error)
	{
		fprintf(stderr, "%s%s\n", prompt, error->error_message);
		unset_error(error);
		return 0;
	}

	return 1;
}

int execute(struct Shell* shell, CommandsList* program)
{
	if (!handle_error(shell->parser->error, "Syntax error: "))
	{
		return 1;
	}

	if (program)
	{
		for (struct Node* node = program->head; node; node = node->next)
		{
			struct AstNode* ast_node = (struct AstNode*)node->data;

			if (ast_node->node_type == AST_PIPELINE_LIST)
			{
				exec_pipeline_list(shell, (PipelinesList*)ast_node->actual_data);
			}
			else // ast_node->node_type == AST_COMPOUND_COMMANDS_LIST
			{
				exec_compound_cmd_list(shell, (CompoundCommandsList*)ast_node->actual_data);
			}

			if (!handle_error(shell->execution_error, "Error: "))
			{
				return 1;
			}
		}
	}

	return 0;
}

void execute_print(struct Shell* shell, CommandsList* program)
{
	if (!handle_error(shell->parser->error, "Syntax error: "))
	{
		return;
	}

	if (program)
	{
		printf("Commands list:\n");

		for (struct Node* n = program->head; n; n = n->next)
		{
			struct AstNode* ast_node = (struct AstNode*)n->data;

			switch (ast_node->node_type)
			{
				case AST_PIPELINE_LIST:
				{
					PipelinesList* pipe_list = (PipelinesList*)ast_node->actual_data;

					if (pipe_list)
					{
						printf("Pipeline list:\n");

						for (struct Node* node = pipe_list->head; node; node = node->next)
						{
							printf("Pipeline:\n");

							struct AstPipeline* pipe = (struct AstPipeline*)node->data;

							pipe->mode == FOREGROUND ? printf("Mode: foreground\n") : printf("Mode: background\n");

							for (struct Node* sc = pipe->pipeline->head; sc; sc = sc->next)
							{
								printf("Command:\n");

								struct AstSimpleCommand* command = sc->data;

								if (command->assignment_list)
								{
									printf("Assignemtns:\n");

									for (struct Node* assignment = command->assignment_list->head; assignment; assignment = assignment->next)
									{
										struct AstAssignment* a = (struct AstAssignment*)assignment->data;

										printf("Variable name: %s\n", a->variable->word.buffer);

										struct AstNode* expr = a->expression;
										struct AstWord* word = NULL;
										struct AstArithmExpr* arithm_expr = NULL;

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
														char** value = get(shell->variables, word->word.word.buffer);

														if (value)
														{
															set_variable(shell, a->variable->word.buffer, *value);
															printf("Expression(PARAMETER_EXPANSION): %s -> %s\n", word->word.word.buffer, *value);
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
											case AST_ARITHM_EXPR:
											{
												char str[12];
												arithm_expr = expr->actual_data;
												sprintf(str, "%d", execute_arithm_expr(shell, arithm_expr));

												if (!handle_error(shell->execution_error, "Error: "))
												{
													return;
												}

												set_variable(shell, a->variable->word.buffer, str);
												printf("Expression(Arithmetic expansion): %s\n", str);
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

									for (struct Node* argument = command->command_args->head; argument; argument = argument->next)
									{
										struct AstNode* node = (struct AstNode*)(argument->data);

										if (node->node_type == AST_WORD)
										{
											struct AstWord* arg = node->actual_data;
											printf("(WORD) %s\n", expand_token(shell, &arg->word));
										}
										else
										{
											struct AstArithmExpr* arithm_expr = node->actual_data;
											printf("(Arithmetic expansion) %d\n", execute_arithm_expr(shell, arithm_expr));

											if (!handle_error(shell->execution_error, "Error: "))
											{
												return;
											}
										}
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
							}
						}
					}
				} break; //case AST_PIPELINE_LIST
				case AST_COMPOUND_COMMANDS_LIST:
				{
					CompoundCommandsList* cc_list = (CompoundCommandsList*)ast_node->actual_data;

					for (struct Node* node = cc_list->head; node; node = node->next)
					{
						struct AstNode* ast_node = (struct AstNode*)node->data;

						switch (ast_node->node_type) // TODO: add AST_FOR and AST_WHILE
						{
							case AST_IF:
							{
								struct AstIf* ast_if = (struct AstIf*)ast_node->actual_data;

								printf("If stamtent:\n");

								printf("Condition: ");
								execute_print(shell, ast_if->condition);

								printf("If part: ");
								execute_print(shell, ast_if->if_part);

								if (ast_if->else_part)
								{
									printf("Else part: ");
									execute_print(shell, ast_if->else_part);
								}
								else
								{
									printf("No else part\n");
								}
							} break;
							case AST_WHILE:
							{
								struct AstWhile* ast_while = (struct AstWhile*)ast_node->actual_data;

								printf("While loop:\n");

								printf("Condition: ");
								execute_print(shell, ast_while->condition);

								printf("Body: ");
								execute_print(shell, ast_while->body);
							} break;
							case AST_FOR:
							{
								struct AstFor* ast_for = (struct AstFor*)ast_node->actual_data;

								printf("For loop:\n");
								printf("Variable: %s\n", ast_for->variable->word.word.buffer);

								char** value = get(shell->variables, ast_for->variable->word.word.buffer);

								if (!value)
								{
									insert(shell->variables, ast_for->variable->word.word.buffer, "");
								}

								printf("Variable's values:\n");

								if (ast_for->wordlist)
								{
									for (struct Node* node = ast_for->wordlist->head; node; node = node->next)
									{
										struct AstNode* expr = (struct AstNode*)node->data;

										switch (expr->node_type)
										{
											case AST_WORD:
											{
												struct AstWord* word = expr->actual_data;
												const char* token_value = expand_token(shell, &word->word);

												printf("(WORD): %s\n", token_value);

												set_variable(shell, ast_for->variable->word.word.buffer, token_value);
											} break;
											case AST_ARITHM_EXPR:
											{
												char str[12];
												struct AstArithmExpr* arithm_expr = expr->actual_data;
												sprintf(str, "%d", execute_arithm_expr(shell, arithm_expr));

												if (shell->execution_error->error)
												{
													fprintf(stderr, "%s\n", shell->execution_error->error_message);
													return;
												}

												printf("(Arithmetic expansion): %s\n", str);

												set_variable(shell, ast_for->variable->word.word.buffer, str);
											} break;
											default: break;
										}
									}
								}

								printf("Body: ");
								execute_print(shell, ast_for->body);

								if (!value)
								{
									erase(shell->variables, ast_for->variable->word.word.buffer);
								}
							} break;
							default: break;
						}
					}
				} break;
				default: break;
			}
		}
	}

	printf("\n");
}