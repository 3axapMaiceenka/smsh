#include "parser.h"
#include "list.h"
#include "utility.h"
#include <stdio.h>

int skip_delim(struct Scanner* scanner)
{
	while (scanner->buffer[scanner->position] == ' ' || scanner->buffer[scanner->position] == '\n' || scanner->buffer[scanner->position] == '\t')
	{
		scanner->position++;
	}

	return scanner->buffer[scanner->position] != '\0';
}

int eat(struct Parser* parser, enum TokenType expected)
{
	if (parser->current_token.type == expected)
	{
		parser->current_token = get_next_token(parser->scanner);
		return 1;
	}

	return 0;
}

struct AstWord* ast_word(struct Parser* parser)
{
	if (parser->current_token.type != WORD)
	{
		return NULL;
	}

	struct AstWord* word = malloc(sizeof(struct AstWord));
	copy_token(&word->word, &parser->current_token);
	eat(parser, WORD);

	return word;
}

// cmd_prefix:   NAME'='WORD
//             | cmd_prefix NAME'='WORD
struct AstAssignmentList* cmd_prefix(struct Parser* parser)
{
	if (parser->current_token.type != NAME)
	{
		return NULL;
	}

	struct AstAssignmentList* assignment_list = calloc(1, sizeof(struct AstAssignmentList));
	assignment_list->assignments = create_list(free_ast_assignment);

	struct AstAssignment* assignment = NULL;
	struct Token token;

	do
	{
		copy_token(&token, &parser->current_token);
		eat(parser, NAME);

		if (parser->current_token.type == WORD) // TODO: add parameter expamsion and arithmetic expanson tokens
		{
			assignment = calloc(1, sizeof(struct AstAssignment));
			assignment->variable = malloc(sizeof(struct Token));
			copy_token(assignment->variable, &token);

			struct AstNode* node = malloc(sizeof(struct AstNode));
			node->node_type = AST_WORD;
			node->actual_data = ast_word(parser); // calls eat

			assignment->expression = node;

			push_back(assignment_list->assignments, assignment);
		}
		else
		{
			destroy_list(&assignment_list->assignments);
			free(assignment_list);
			set_error(parser->error, "Error: expected WORD token\n");			

			return NULL;
		}
	} while (parser->current_token.type == NAME);

	return assignment_list;
}

// filenme: WORD
struct AstWord* filename(struct Parser* parser)
{
	return ast_word(parser);
}

/*
io_redirect:   '<' filename
			 | '>' filename
*/
struct AstIORedirect* io_redirect(struct Parser* parser)
{
	if (parser->current_token.type == INPUT_REDIRECT || parser->current_token.type == OUTPUT_REDIRECT)
	{
		struct Token* token = malloc(sizeof(struct Token));
		copy_token(token, &parser->current_token);
		eat(parser, parser->current_token.type);

		struct AstWord* file_name = filename(parser);
		if (!file_name) 
		{
			free(token);
			set_error(parser->error, "Incomplete I/O redirection! Expected filename!\n");

			return NULL;
		}

		struct AstIORedirect* io_redir = malloc(sizeof(struct AstIORedirect));
		io_redir->file_name = file_name;
		io_redir->token = token;

		return io_redir;
	}

	return NULL;
}

/*
cmd_suffix:   io_redirect
			| cmd_suffix io_redirect
			| WORD
			| cmd_suffix WORD
*/
struct CmdArgs* cmd_suffix(struct Parser* parser)
{
	struct CmdArgs* cmd_args = calloc(1, sizeof(struct CmdArgs));

	for (int running = 1; running; )
	{
		struct AstIORedirect* io_redir = io_redirect(parser);

		if (io_redir)
		{
			if (io_redir->token->type == OUTPUT_REDIRECT)
			{
				free_ast_io_redirect(cmd_args->output_redirect);
				cmd_args->output_redirect = io_redir;
			}
			else
			{
				free_ast_io_redirect(cmd_args->input_redirect);
				cmd_args->input_redirect = io_redir;
			}
		}
		else
		{
			if (parser->error->error)
			{
				return cmd_args;
			}

			struct AstWord* arg = ast_word(parser);

			if (arg)
			{
				if (!cmd_args->command_args)
				{
					cmd_args->command_args = calloc(1, sizeof(struct AstWordlist));
					cmd_args->command_args->wordlist = create_list(&free_ast_word);
				}

				push_back(cmd_args->command_args->wordlist, arg);
			}
			else
			{
				running = 0;
			}
		}
	}

	if (!cmd_args->input_redirect && !cmd_args->output_redirect && !cmd_args->command_args)
	{
		free(cmd_args);
		return NULL;
	}

	return cmd_args;
}

// cmd_name: WORD
struct AstWord* cmd_name(struct Parser* parser)
{
	return ast_word(parser);
}

/*
simple_command:   cmd_prefix cmd_name cmd_suffix
				| cmd_prefix cmd_name
				| cmd_prefix
				| cmd_name cmd_suffix
				| cmd_name
*/
struct AstSimpleCommand* simple_command(struct Parser* parser)
{
	struct AstSimpleCommand* ast_scommand = NULL;

	struct AstAssignmentList* assignment_list = cmd_prefix(parser);
	if (assignment_list)
	{
		ast_scommand = calloc(1, sizeof(struct AstSimpleCommand));
		ast_scommand->assignment_list = assignment_list;
	}
	else
	{
		if (parser->error->error)
		{
			return NULL;
		}
	}

	struct AstWord* command_name = cmd_name(parser);
	if (command_name)
	{
		if (!ast_scommand)
		{
			ast_scommand = calloc(1, sizeof(struct AstSimpleCommand));
		}

		ast_scommand->command_name = command_name;
	}
	else
	{
		if (!ast_scommand) return NULL;
	}

	if (ast_scommand->command_name)
	{
		struct CmdArgs* cmd_args = cmd_suffix(parser);

		if (cmd_args)
		{
			ast_scommand->command_args = cmd_args->command_args;
			ast_scommand->input_redirect = cmd_args->input_redirect;
			ast_scommand->output_redirect = cmd_args->output_redirect;
		}
	}

	return ast_scommand;
}

struct AstSimpleCommand* parse(struct Parser* parser)
{
	if (parser->current_token.type == END)
	{
		return NULL;
	}

	return simple_command(parser);
}

void init_buffer(struct Buffer* buffer)
{
	buffer->size = 0;
	buffer->capacity = BUF_CAP;
	buffer->buffer = (char*)malloc(BUF_CAP);
}

void append_char(struct Buffer* buffer, char c)
{
	if (buffer->size >= buffer->capacity)
	{
		buffer->capacity <<= 1;
		buffer->buffer = (char*)realloc(buffer->buffer, buffer->capacity);
	}

	buffer->buffer[buffer->size++] = c;
}

struct Token get_next_token(struct Scanner* scanner)
{
	struct Token token;
	token.type = END;

	if (!skip_delim(scanner))
	{
		return token;
	}

	token.type = WORD;
	init_buffer(&token.word);

	const char* buffer = scanner->buffer;
	size_t position = scanner->position;
	int contains_quotes = 0;

	for (int running = 1; running; )
	{
		char c = buffer[position++];

		if (c == 39 || c == 34) // single or double quote
		{
			while (buffer[position] != c && buffer[position] != '\0')
			{
				append_char(&token.word, buffer[position++]);
			}

			if (buffer[position] == '\0')
			{
				free(token.word.buffer);
				return token;
			}

			contains_quotes = 1;
			position++;
		}
		else
		{
			if (c == '=' && !contains_quotes && token.word.size && buffer[position] != ' ' && buffer[position] != '\n' && buffer[position] != '\0' && buffer[position] != '\t')
			{
				token.type = NAME;
				running = 0;
			}
			else
			{
				append_char(&token.word, c);
			}
		}

		if (buffer[position] == ' ' || buffer[position] == '\n' || buffer[position] == '\t' || buffer[position] == '\0')
		{
			break;
		}
	}

	append_char(&token.word, '\0');
	scanner->position = position;

	return token;
}

void set_error(struct Error* error, const char* message)
{
	error->error_message = copy_string(message);
	error->error = 1;
}

void copy_token(struct Token* dest, struct Token* src)
{
	dest->type = src->type;
	dest->word.buffer = src->word.buffer;
	dest->word.capacity = src->word.capacity;
	dest->word.size = src->word.size;
}

void free_ast_simple_command(void* ast_scommand)
{
	if (ast_scommand)
	{
		struct AstSimpleCommand* ast_command = (struct AstSimpleCommand*)ast_scommand;
		free_ast_assignment_list(ast_command->assignment_list);
		free_ast_io_redirect(ast_command->input_redirect);
		free_ast_io_redirect(ast_command->output_redirect);
		free_ast_wordlist(ast_command->command_args);
		free_ast_word(ast_command->command_name);
		free(ast_command);
	}
}

void free_ast_assignment_list(void* assignment_list)
{
	if (assignment_list)
	{
		struct AstAssignmentList* list = (struct AstAssignmentList*)assignment_list;
		destroy_list(&list->assignments);
		free(list);
	}
}

void free_ast_assignment(void* assignment)
{
	if (assignment)
	{
		struct AstAssignment* ast_assignment = (struct AstAssignment*)assignment;
		free_ast_node(ast_assignment->expression);
		free(ast_assignment->variable->word.buffer);
		free(ast_assignment->variable);
		free(ast_assignment);
	}
}

void free_ast_node(void* node)
{
	if (node)
	{
		struct AstNode* n = (struct AstNode*)node;

		switch (n->node_type)
		{
			case AST_WORD:
			{
				free_ast_word(n->actual_data);
			} break;
			case AST_WORDLIST:
			{
				free_ast_wordlist(n->actual_data);
			} break;
			case AST_SIMPLE_COMMAND:
			{
				free_ast_simple_command(n->actual_data);
			} break;
			case AST_ASSIGNMENT:
			{
				free_ast_assignment(n->actual_data);
			} break;
			default:
			{
				free(n->actual_data);
			} break;
		}

		free(n);
	}
}

void free_ast_io_redirect(void* io_redir)
{
	if (io_redir)
	{
		struct AstIORedirect* redir = (struct AstIORedirect*)io_redir;
		free_ast_word(redir->file_name);
		free(redir->token);
		free(redir);
	}
}

void free_ast_word(void* word)
{
	if (word)
	{
		struct AstWord* w = (struct AstWord*)word;
		free(w->word.word.buffer);
		free(w);
	}
}

void free_ast_wordlist(void* wordlist)
{
	if (wordlist)
	{
		struct AstWordlist* list = (struct AstWordlist*)wordlist;
		destroy_list(&list->wordlist);
		free(list);
	}
}