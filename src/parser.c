#include "parser.h"
#include "list.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void initialize(struct Parser* parser, char* buffer)
{
	parser->scanner.buffer = buffer;
	parser->scanner.position = 0;
	parser->current_token = get_next_token(&parser->scanner);
}

int skip_delim(struct Scanner* scanner)
{
	while (scanner->buffer[scanner->position] == ' ' || scanner->buffer[scanner->position] == '\n' || scanner->buffer[scanner->position] == '\t')
	{
		scanner->position++;
	}

	return scanner->buffer[scanner->position] != '\0';
}

int read_quoted_string(struct Scanner* scanner, struct Token* token)
{
	token->type = QUOTED_STR;
	token->nodes = token->next = NULL;
	token->word_start = scanner->position;

	const char* buffer = scanner->buffer;
	size_t position = scanner->position;

	while (buffer[position] != 39 && buffer[position] != '\0')
	{
		position++;
	}

	if (buffer[position] == '\0') // syntax error
	{
		return -1;
	}

	int ret_val = (position != scanner->position); // returns 0 if single-quoted string is empty, else returns 1

	scanner->position = position + 1;
	token->word_end = position - 1;

	return ret_val;
}

void emplace_token(struct Token* token, struct Token* node)
{
	struct Token* new_node = malloc(sizeof(struct Token));
	new_node->nodes = node->nodes;
	new_node->type = node->type;
	new_node->word_end = node->word_end;
	new_node->word_start = node->word_start;
	new_node->next = NULL;

	if (!token->nodes)
	{
		token->nodes = new_node;
		return;
	}

	struct Token* temp = token->nodes;
	for (; temp->next; temp = temp->next)
		;

	temp->next = new_node;
}

int quoted_string(struct Scanner* scanner, struct Token* parent, struct Token* token)
{
	int rc = read_quoted_string(scanner, token);

	if (rc == 1 && parent)
	{	
		emplace_token(parent, token);
	}
	
	return rc;
}

struct Token get_next_token(struct Scanner* scanner)
{
	struct Token token;
	token.type = END;
	token.nodes = token.next = NULL;

	if (!skip_delim(scanner))
	{
		return token;
	}

	struct Token node;
	const char* buffer = scanner->buffer;
	size_t position = scanner->position;

	if (buffer[position] == 39)
	{
		scanner->position++;

		if (quoted_string(scanner, NULL, &node) == -1) // handle
		{
			return token;
		}
		
		position = scanner->position;
		char c = buffer[position];

		if (c == ' ' || c == '\n' || c == '\t' || c == '\0')
		{
			return node;
		}
		else
		{
			token.word_start = node.word_start;
			emplace_token(&token, &node);
		}
	}
	else
	{
		token.word_start = position;
	}
	
	token.type = WORD;

	for (;;)
	{
		if (buffer[position++] == 39)
		{
			scanner->position = position;

			if (quoted_string(scanner, &token, &node) == -1) // handle properly
			{
				fprintf(stderr, "No terminating quote\n");
			}

			position = scanner->position;
		}

		char c = buffer[position];
		if (c == ' ' || c == '\n' || c == '\t' || c == '\0')
		{
			if (scanner->position == position) // if quote is the last symbol before c
			{
				token.word_end = position - 2;
			}
			else
			{
				token.word_end = position - 1;
				scanner->position = position;
			}

			break;
		}
	}

	return token;
}

int eat(struct Parser* parser, enum TokenType expected)
{
	if (parser->current_token.type == expected)
	{
		parser->current_token = get_next_token(&parser->scanner);
		return 1;
	}

	return 0;
}

void copy_token(struct Token* dest, struct Token* src)
{
	dest->nodes = src->nodes;
	dest->type = src->type;
	dest->word_end = src->word_end;
	dest->word_start = src->word_start;
	dest->next = src->next;
}

struct AstWord* ast_word(struct Parser* parser)
{
	struct AstWord* word = NULL;

	if (parser->current_token.type == WORD || parser->current_token.type == QUOTED_STR)
	{
		word = malloc(sizeof(struct AstWord));
		copy_token(&word->word, &parser->current_token);
		eat(parser, parser->current_token.type);
	}

	return word;
}

/*
cmd_suffix: WORD
			| cmd_suffix WORD
*/
struct AstWordlist* cmd_suffix(struct Parser* parser)
{
	if (parser->current_token.type != WORD && parser->current_token.type != QUOTED_STR)
	{
		return NULL;
	}

	struct AstWordlist* ast_wordlist = calloc(1, sizeof(struct AstWordlist));

	do 
	{
		struct AstWord* word = ast_word(parser);
		push_back(&ast_wordlist->wordlist, word);
	} while (parser->current_token.type == WORD || parser->current_token.type == QUOTED_STR);

	return ast_wordlist;
}

// cmd_name: WORD
struct AstWord* cmd_name(struct Parser* parser)
{
	return ast_word(parser);
}

/*
simple_command: cmd_name cmd_suffix
				| cmd_name
*/
struct AstSimpleCommand* simple_command(struct Parser* parser)
{
	struct AstSimpleCommand* ast_scommand = NULL;

	struct AstWord* name = cmd_name(parser);
	if (!name)
	{
		return NULL;
	}

	ast_scommand = malloc(sizeof(struct AstSimpleCommand));
	ast_scommand->command_name = name;
	ast_scommand->args = cmd_suffix(parser);

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

char* expand_quoted_str(struct Token* token, const char* buffer)
{
	size_t size = sizeof(char) * (token->word_end - token->word_start + 2);
	char* expanded = malloc(size);
	memcpy(expanded, buffer + token->word_start, size - 1);
	expanded[size - 1] = '\0';

	return expanded;
}

char* copy_memory(char* buffer, size_t* buf_size, size_t pos, const char* src, size_t src_size)
{
	if (*buf_size - pos < src_size)
	{
		*buf_size = pos + src_size;
		buffer = realloc(buffer, *buf_size);
	}

	memcpy(buffer + pos, src, src_size);

	return buffer;
}

char* expand_word(struct Token* token, const char* buffer)
{
	size_t buf_size = STRING_SIZE;
	size_t src_size = 0, curr_size = 0;;
	char* expanded = malloc(STRING_SIZE);

	if (token->nodes)
	{
		size_t prev_end = token->word_start;

		if (token->word_start < token->nodes->word_start)
		{
			src_size = token->nodes->word_start - token->word_start - 1; // quote 
			expanded = copy_memory(expanded, &buf_size, 0, buffer + token->word_start, src_size);
			curr_size = src_size;
			prev_end = src_size;
		}

		struct Token* t = token->nodes;
		for (;; t = t->next)
		{
			if (t->word_start > prev_end + 1)
			{
				src_size = t->word_start - prev_end - 3;
				expanded = copy_memory(expanded, &buf_size, curr_size, buffer + prev_end + 2, src_size);
				curr_size += src_size;
			}

			if (t->type == QUOTED_STR)
			{
				char* substr = expand_quoted_str(t, buffer);
				src_size = t->word_end - t->word_start + 1;
				expanded = copy_memory(expanded, &buf_size, curr_size, substr, src_size);
				curr_size += src_size;
				prev_end = t->word_end;
			}

			if (!t->next)
			{
				break;
			}
		}

		if (token->word_end != t->word_end)
		{
			src_size = token->word_end - t->word_end - 1;
			expanded = copy_memory(expanded, &buf_size, curr_size, buffer + t->word_end + 2, src_size);
			curr_size += src_size;
		}
	}
	else
	{
		src_size = token->word_end - token->word_start + 1;
		expanded = copy_memory(expanded, &buf_size, 0, buffer + token->word_start, src_size);
		curr_size += src_size;
	}

	expanded = realloc(expanded, curr_size + 1);
	expanded[curr_size] = '\0';

	return expanded;
}

char* expand_token(struct Token* token, const char* buffer)
{
	switch (token->type)
	{
		case QUOTED_STR:
		{
			return expand_quoted_str(token, buffer);
		}
		case WORD:
		{
			return expand_word(token, buffer);
		}
		// TODO: add arithmetic expansion and parameter expansion
		default:
		{
			return NULL;
		}
	}
}

void execute(struct AstSimpleCommand* command, const char* buffer)
{
	if (command)
	{
		if (command->command_name)
		{
			char* name = expand_token(&command->command_name->word, buffer);

			fprintf(stdout, "Command name: %s\n", name);

			destroy_token(&command->command_name->word);
			free(command->command_name);
			free(name);
		}

		if (command->args)
		{
			for (struct List* arg = command->args->wordlist; arg; arg = arg->next)
			{
				struct AstWord* comm_arg = (struct AstWord*)arg->data;
				char* args = expand_token(&comm_arg->word, buffer);

				fprintf(stdout, "Arguments: %s\n", args);

				free(args);
				destroy_token(&comm_arg->word);
			}

			destroy_list(&command->args->wordlist);
			free(command->args);
		}

		free(command);

		fprintf(stdout, "\n");
	}
}

void destroy_token(struct Token* token)
{
	if (token->nodes)
	{
		destroy_token(token->nodes);
		free(token->nodes);
	}

	if (token->next)
	{
		destroy_token(token->next);
		free(token->next);
	}
}