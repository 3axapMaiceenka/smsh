#include "shell.h"
#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Shell* start()
{
	struct Shell* shell = calloc(1, sizeof(struct Shell));
	shell->parser = calloc(1, sizeof(struct Parser));
	shell->scanner = calloc(1, sizeof(struct Scanner));
	shell->execution_error = calloc(1, sizeof(struct Error));
	shell->variables = create_hashtable(DEFAULT_HASHTABLE_SIZE);

	shell->parser->scanner = shell->scanner;
	shell->parser->error = calloc(1, sizeof(struct Error));

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
	if (*string == '-' || (*string >= '1' && *string <= '9'))
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
					set_error(shell->execution_error, "Execution error! Invalid parameter expansion inside arithmetic expansion!");
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
		default:
		{
		} break;
	}

	return 0;
}

void execute(struct Shell* shell, CommandsList* program)
{
	if (shell->parser->error->error)
	{
		fprintf(stderr, "Syntax error: %s\n", shell->parser->error->error_message);
		destroy_list(&program);
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

												if (shell->execution_error->error)
												{
													fprintf(stderr, "%s\n", shell->execution_error->error_message);
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

											if (shell->execution_error->error)
											{
												fprintf(stderr, "%s\n", shell->execution_error->error_message);
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
								execute(shell, ast_if->condition);

								printf("If part: ");
								execute(shell, ast_if->if_part);

								if (ast_if->else_part)
								{
									printf("Else part: ");
									execute(shell, ast_if->else_part);
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
								execute(shell, ast_while->condition);

								printf("Body: ");
								execute(shell, ast_while->body);
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