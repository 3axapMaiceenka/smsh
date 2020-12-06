#include "shell.h"
#include "utility.h"
#include "builtin.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IO_REDIRECT_ERROR_MES "can't execute I/O redirection"

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

const char* get_variable(struct Shell* shell, const char* var_name)
{
	char** value = get(shell->variables, var_name);

	if (value)
	{
		return *value;
	}
	
	return NULL;
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
			const char* value = get_variable(shell, token->word.buffer);
			
			if (!value)
			{
				return "";
			}
			
			return value;
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

static char* expand_arithm_expr(struct Shell* shell, struct AstArithmExpr* arithm_expr)
{
	char* exp = calloc(12, sizeof(char)); // int is maximum 11 digits long
	sprintf(exp, "%d", execute_arithm_expr(shell, arithm_expr));
	
	if (shell->execution_error->error)
	{
		free(exp);
		return NULL;		
	}

	return exp;
}

// The exit status of a for command shall be the exit status of the last command that executes
static int exec_for_loop(struct Shell* shell, struct AstFor* ast_for)
{
	const char* var_name = ast_for->variable->word.word.buffer;
	char** value = get(shell->variables, var_name);
	char* init_value = NULL;
	int rc = 0;

	if (!value)
	{
		insert(shell->variables, var_name, "");
	}
	else
	{
		init_value = copy_string(*value);
	}

	for (struct Node* node = ast_for->wordlist->head; node; node = node->next)
	{
		struct AstNode* expr = (struct AstNode*)node->data;
	
		if (expr->node_type == AST_WORD)
		{
			struct AstWord* word = expr->actual_data;
			const char* token_value = expand_token(shell, &word->word);
			set_variable(shell, var_name, token_value);
		}
		else // expr->node_type == AST_ARITHM_EXPR
		{		
			char* var_value = expand_arithm_expr(shell, (struct AstArithmExpr*)expr->actual_data);
	
			if (shell->execution_error->error)
			{
				return 1;
			}

			set_variable(shell, var_name, var_value);
			free(var_value);
		}

		rc = execute(shell, ast_for->body);

		if (shell->execution_error->error)
		{
			return rc;
		}
	}

	if (!value)
	{
		erase(shell->variables, var_name);
	}
	else
	{
		set_variable(shell, var_name, init_value);
		free(init_value);
	}

	return rc;
}

// The exit status of the while loop shall be the exit status of the last body commands list executed, or zero, if none was executed.
static int exec_while_loop(struct Shell* shell, struct AstWhile* ast_while)
{
	int rc = 0;
	int cond = 0;

	while ((cond = execute(shell, ast_while->condition)) == 0)
	{
		rc = execute(shell, ast_while->body);

		if (shell->execution_error->error)
		{
			return rc;
		}
	}

	if (shell->execution_error->error)
	{
		return cond;
	}

	return rc;
}

// The exit status of the if command shall be the exit status of the then or else commands list that was executed, or zero, if none was executed
static int exec_if(struct Shell* shell, struct AstIf* ast_if)
{
	int rc = execute(shell, ast_if->condition);

	if (!rc)
	{
		rc = execute(shell, ast_if->if_part);
	}
	else
	{
		if (shell->execution_error->error)
		{
			return rc;
		}

		rc = execute(shell, ast_if->else_part);
	}

	return rc;
}

static int exec_compound_cmd_list(struct Shell* shell, CompoundCommandsList* cmd_list)
{
	int rc = 0;

	for (struct Node* node = cmd_list->head; node; node = node->next)
	{
		struct AstNode* ast_node = (struct AstNode*)node->data;

		switch (ast_node->node_type)
		{
			case AST_WHILE:
			{
				rc = exec_while_loop(shell, (struct AstWhile*)ast_node->actual_data);
			} break;
			case AST_IF:
			{
				rc = exec_if(shell, (struct AstIf*)ast_node->actual_data);
			} break;
			case AST_FOR:
			{
				rc = exec_for_loop(shell ,(struct AstFor*)ast_node->actual_data);
			} break;
			default: break; // never executed
		}

		if (shell->execution_error->error)
		{
			break;
		}
	}

	return rc;
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
		char* arithm_expr = expand_arithm_expr(shell, (struct AstArithmExpr*)assignment->expression->actual_data);

		if (!shell->execution_error->error)
		{
			set_variable(shell, expand_token(shell, assignment->variable), arithm_expr);
			free(arithm_expr);
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

// if fd is INPUT_FILENO or OUTPUT_FILENO or ins't valid file descriptor won't close it 
static void close_fd_safely(int fd)
{
	if (fd != 0 && fd != 1 && fd != -1)
	{
		close(fd);
	}
}

static int start_process(const char* cmd_name, char** cmd_args, int infd, int outfd, int wait)
{
	pid_t pid = fork();

	if (pid == -1)
	{
		return -1;
	}

	if (pid == 0)
	{
		int r1 = dup2(infd, 0);
		int r2 = dup2(outfd, 1);

		close_fd_safely(infd); // if infd is INPUT_FILENO or -1 won't close 
		close_fd_safely(outfd); // if outfd is OUTPUT_FILENO or -1 won't close 

		if (r1 == -1 || r2 == -1)
		{
			return 1; // If a command fails during word expansion or redirection, its exit status shall be greater than zero
		}

		if (execv(cmd_name, cmd_args) == -1)
		{
			return 127; // If a command is not found, the exit status shall be 127
		}
	}
	else
	{
		if (wait)
		{
			int status;

			do
			{
				waitpid(pid, &status, WUNTRACED);

				if (WIFEXITED(status))
				{
					return WEXITSTATUS(status);
				}

				if (WIFSIGNALED(status))
				{
					return 129; //  The exit status of a command that terminated because it received a signal shall be reported as greater than 128
				}
			} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		}
	}

	return 0;
}

static void free_cmd_args(char** argv)
{
	if (argv)
	{
		for (char** ptr = argv; *ptr; ptr++)
		{
			free(*ptr);
		}
	
		free(argv);
	}
}

static int wordlist_to_array(struct Shell* shell, char** array, Wordlist* wordlist, size_t indx)
{
	if (wordlist)
	{
		for (struct Node* node = wordlist->head; node; node = node->next)
		{
			struct AstNode* ast_node = (struct AstNode*)node->data;

			if (ast_node->node_type == AST_WORD)
			{
				struct AstWord* ast_word = (struct AstWord*)ast_node->actual_data;
				array[indx++] = copy_string(expand_token(shell, &ast_word->word));
			}
			else // ast_node->node_type == AST_ARITHM_EXPR
			{
				char* arithm_expr = expand_arithm_expr(shell, (struct AstArithmExpr*)ast_node->actual_data);
		
				if (shell->execution_error->error)
				{
					free_cmd_args(array);
					return 0;
				}

				array[indx++] = arithm_expr;
			}
		}
	}

	return 1;
}

static char** create_builtin_args(struct Shell* shell, Wordlist* command_args)
{
	char** argv = calloc(get_list_size(command_args) + 1, sizeof(char*));

	if (!wordlist_to_array(shell, argv, command_args, (size_t)0))
	{
		return NULL;
	}

	return argv;
}

static char** create_cmd_args(struct Shell* shell, const char* cmd_name, Wordlist* command_args)
{
	char** argv = calloc(get_list_size(command_args) + 2, sizeof(char*));
	argv[0] = copy_string(cmd_name);

	if (!wordlist_to_array(shell, argv, command_args, (size_t)1))
	{
		return NULL;
	}

	return argv;
}

static int get_io_redir_fd(struct Shell* shell, struct AstIORedirect* ast_redirect)
{
	const char* path = expand_token(shell, &ast_redirect->file_name->word);
	int fd = -1;

	if (ast_redirect->token->type == INPUT_REDIRECT)
	{
		fd = open(path, O_RDONLY, S_IRWXU);
	}
	else // ast_redirect->token->type == OUTPUT_REDIRECT
	{
		fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU); // create if doesn't exist or truncate
	}

	return fd;
}

static void handle_io_redir_error(struct Shell* shell, int fd)
{
	set_error(shell->execution_error, IO_REDIRECT_ERROR_MES);
	close_fd_safely(fd);
}

static void handle_unexisting_cmd_error(struct Shell* shell, const char* path)
{
	char* error_mes = concat_strings(path, " - command not found");
	set_error(shell->execution_error, error_mes);
	free(error_mes);
}

static char* find_executable(struct Shell* shell, const char* exec_name)
{
	if (strchr(exec_name, '/'))
	{
		return copy_string(exec_name);
	}

	const char* path = get_variable(shell, "PATH");
	char* _path = NULL;

	if (!path) // if PATH isn't defined, the path list defaults to the current directory followed by the list of directories returned by confstr(_CS_PATH)
	{
		size_t len = confstr(_CS_PATH, NULL, (size_t)0);
		if (!len) // path is undefined
		{
			return NULL;
		}

		_path = malloc(sizeof(char) * len);
		confstr(_CS_PATH, _path, len);
	}
	else
	{
		_path = copy_string(path);
	}

	char* dir = strtok(_path, ":");
	char* full_path = NULL;
	size_t exec_name_len = strlen(exec_name);

	while (dir)
	{
		size_t len = strlen(dir);
		full_path = calloc(exec_name_len + len + 2, sizeof(char));

		strcat(full_path, dir);
		full_path[len] = '/';
		strcat(full_path, exec_name);

		if (access(full_path, X_OK) == 0)
		{
			break;
		}

		dir = strtok(NULL, ":");
		free(full_path);
		full_path = NULL;
	}

	free(_path);

	return full_path;
}

// new_fd is a pointer to stdandart input file descriptor, fd is standart output file descriptor or vice versa
static int redirect_io(struct Shell* shell, struct AstIORedirect* ast_io_redir, int* new_fd, int fd)
{
	if (ast_io_redir)
	{
		close_fd_safely(*new_fd);

		if ((*new_fd = get_io_redir_fd(shell, ast_io_redir)) == -1)
		{
			handle_io_redir_error(shell, fd); // closes fd and sets error
			return -1;
		}
	}

	return 0;
}

static int exec_simple_command(struct Shell* shell, struct AstSimpleCommand* simple_command, int infd, int outfd, int wait)
{	
	if (simple_command->assignment_list)
	{
		exec_assignments_list(shell, simple_command->assignment_list);
	}

	if (shell->execution_error->error)
	{
		return -1;
	}

	if (simple_command->command_name)
	{
		const char* cmd_name = expand_token(shell, &simple_command->command_name->word);

		const struct Builtin* const builtin = is_builtin(cmd_name);
		if (builtin)
		{
			char** builtin_args = create_builtin_args(shell, simple_command->command_args);
			int rc = 1;

			if (!shell->execution_error->error)
			{
				rc = builtin->exec(shell, builtin_args);
			}

			close_fd_safely(infd);
			close_fd_safely(outfd);
			free_cmd_args(builtin_args);
			return rc;
		} 

		char* path = find_executable(shell, cmd_name);	
		if (!path)
		{
			handle_unexisting_cmd_error(shell, cmd_name);
			close_fd_safely(infd);
			close_fd_safely(outfd);
			return 127;
		}

		char** cmd_args = create_cmd_args(shell, path, simple_command->command_args);
		if (shell->execution_error->error)
		{
			close_fd_safely(infd);
			close_fd_safely(outfd);
			return 1; // If a command fails during word expansion or redirection, its exit status shall be greater than zero
		}

		if (redirect_io(shell, simple_command->input_redirect, &infd, outfd) == -1)
		{
			free_cmd_args(cmd_args);
			free(path);
		}

		if (redirect_io(shell, simple_command->output_redirect, &outfd, infd) == -1)
		{
			free_cmd_args(cmd_args);
			free(path);
		}

		int rc = start_process(path, cmd_args, infd, outfd, wait);		

		switch (rc)
		{
			case -1: // fork() failed
			{
				set_error(shell->execution_error, "can't create a child process");
			} break;
			case 127: // execv() failed
			{
				handle_unexisting_cmd_error(shell, path);
			} break;
			default: break; // child process terminated or was killed by a signal
		}

		close_fd_safely(infd);
		close_fd_safely(outfd);
		free_cmd_args(cmd_args);
		free(path);

		return rc;
	}

	return 0;
}

static int exec_pipeline(struct Shell* shell, struct AstPipeline* pipeline)
{
	//TODO: add waiting for all pipeline's commands to terminate
	if (pipeline) 
	{
		int wait = 0;
		int rc = 0;
		int pipefd[2] = { 0, 1 };

		for (struct Node* node = pipeline->pipeline->head; node; node = node->next)
		{
			int infd = pipefd[0];

			if (node->next) // if isn't the last command in the pipeline
			{
				pipe(pipefd);
			}
			else
			{
				// If the pipeline is not in the background, the shell shall wait for the last command specified in the pipeline to complete
				if (pipeline->mode == FOREGROUND) 
				{
					wait = 1;
				}

				pipefd[1] = 1;
			}

			rc = exec_simple_command(shell, (struct AstSimpleCommand*)node->data, infd, pipefd[1], wait); // fds are closed

			if (shell->execution_error->error)
			{
				break;
			}
		}

		return rc; // the exit status shall be the exit status of the last command specified in the pipeline
	}

	return 0;
}

static int exec_pipeline_list(struct Shell* shell, PipelinesList* pipe_list)
{
	// The exit status of an asynchronous list shall be zero
	// The exit status of a sequential list shall be the exit status of the last command in the list
	if (pipe_list)
	{
		int rc = 0;

		for (struct Node* node = pipe_list->head; node; node = node->next)
		{
			rc = exec_pipeline(shell, (struct AstPipeline*)node->data); // if pipeline is running in the background mode, exec_pipeline() returns 0

			if (shell->execution_error->error)
			{
				break;
			}
		}

		return rc;
	}

	return 0;
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
	if (program)
	{
		int rc = 0;

		for (struct Node* node = program->head; node; node = node->next)
		{
			struct AstNode* ast_node = (struct AstNode*)node->data;

			if (ast_node->node_type == AST_PIPELINE_LIST)
			{
				rc = exec_pipeline_list(shell, (PipelinesList*)ast_node->actual_data);
			}
			else // ast_node->node_type == AST_COMPOUND_COMMANDS_LIST
			{
				rc = exec_compound_cmd_list(shell, (CompoundCommandsList*)ast_node->actual_data);
			}

			if (shell->execution_error->error)
			{
				return rc;
			}
		}

		return rc;
	}

	return 0;
}

int shell_execute(struct Shell* shell, char* buffer)
{
	initialize(shell, buffer);
	shell->program = parse(shell->parser);

	int rc = 1;

	if (handle_error(shell->parser->error, "Syntax error: "))
	{
		rc = execute(shell, shell->program);
		handle_error(shell->execution_error, "Error: ");	
	}

	destroy_list(&shell->program);

	return rc;
}

int shell_execute_from_file(struct Shell* shell, const char* filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "Failed to open %s\n", filename);
		return 1;
	}

	// skip schebang
	while (1)
	{
		char c;

		ssize_t n = read(fd, &c, 1);
		if (n != 1)
		{
			close(fd);
			return 1;
		}

		if (c == ' ' || c == '\t' || c == '\n')
		{
			break;
		}
	}

	off_t beg = lseek(fd, 0, SEEK_CUR);
	if (beg == -1)
	{
		close(fd);
		return 1;
	}

	off_t end = lseek(fd, 0, SEEK_END);
	if (end == -1)
	{
		close(fd);
		return 1;
	}

	if (lseek(fd, beg, SEEK_SET) == -1)
	{
		close(fd);
		return 1;
	}

	size_t buf_size = (size_t)(end - beg + 1);
	char* buffer = calloc(end - beg + 1, sizeof(char));

	ssize_t n = read(fd, buffer, buf_size - 1);
	if (n == -1)
	{
		close(fd);
		return 1;
	}

	int rc = shell_execute(shell, buffer);

	close(fd);
	free(buffer);

	return rc;
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
												arithm_expr = expr->actual_data;
												char* str = expand_arithm_expr(shell, arithm_expr);

												if (!handle_error(shell->execution_error, "Error: "))
												{
													return;
												}

												set_variable(shell, a->variable->word.buffer, str);
												printf("Expression(Arithmetic expansion): %s\n", str);
												free(str);
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
												char* str = expand_arithm_expr(shell, expr->actual_data);

												if (shell->execution_error->error)
												{
													fprintf(stderr, "%s\n", shell->execution_error->error_message);
													return;
												}

												printf("(Arithmetic expansion): %s\n", str);

												set_variable(shell, ast_for->variable->word.word.buffer, str);
												free(str);
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