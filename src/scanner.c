#include "scanner.h"

void init_buffer(struct Buffer* buffer)
{
	buffer->size = 0;
	buffer->capacity = BUF_CAP;
	buffer->buffer = malloc(BUF_CAP);
}

void append_char(struct Buffer* buffer, char c)
{
	if (buffer->size >= buffer->capacity)
	{
		buffer->capacity <<= 1;
		buffer->buffer = realloc(buffer->buffer, buffer->capacity);
	}

	buffer->buffer[buffer->size++] = c;
}

// reads symbols from buffer from position until encountered terminating quote(returns 1 in that case) or '\0'(returns 0)
int handle_quotes(char quote, size_t* position, const char* buffer, struct Token* token, int* contains_quotes)
{
	while (buffer[*position] != quote && buffer[*position] != '\0')
	{
		append_char(&token->word, buffer[(*position)++]);
	}

	if (buffer[*position] == '\0')
	{
		token->type = END;
		return 0;
	}

	*contains_quotes = 1;
	(*position)++;

	return 1;
}

int handle_io_redirect(char redirect, const char* buffer, size_t* position, int contains_quotes, struct Token* token)
{
	if (contains_quotes)
	{
		append_char(&token->word, redirect);
		return 1;
	}

	if (!token->word.size)
	{
		token->type = redirect == '<' ? INPUT_REDIRECT : OUTPUT_REDIRECT;
	}
	else
	{
		if (buffer[*position] != ' ' && buffer[*position] != '\t' && buffer[*position] != '\0' && buffer[*position] != '\n')
		{
			(*position)--;
		}
	}

	return 0;
}

static void free_buffer(struct Buffer* buffer)
{
	free(buffer->buffer);
	buffer->buffer = NULL;
}

static void free_token(struct Token* token)
{
	free_buffer(&token->word);
	token->type = END;
}

struct Token get_next_token(struct Scanner* scanner, int* arithm_expr_beginning)
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

		switch (c)
		{
			case '$':
			{
				if (!token.word.size)
				{
					if (buffer[position] == '(' && buffer[position + 1] == '(') // $((..
					{
						free_token(&token);
						*arithm_expr_beginning = 1;
						scanner->position = position + 2;
						return token;
					}

					token.type = PARAMETER_EXPANSION;
				}
				else
				{
					running = 0;
					position--;
				}
			} break;
			case '<':
			{
				running = handle_io_redirect(c, buffer, &position, contains_quotes, &token);
			} break;
			case '>':
			{
				running = handle_io_redirect(c, buffer, &position, contains_quotes, &token);
			} break;
			case '=':
			{
				if (!contains_quotes && token.word.size && buffer[position] != ' ' && buffer[position] != '\n'
					&& buffer[position] != '\0' && buffer[position] != '\t')
				{
					token.type = NAME;
					running = 0;
				}
				else
				{
					append_char(&token.word, c);
				}
			} break;
			case '|':
			{
				token.type = PIPE;
				running = 0;
			} break;
			case '\n':
			{
				token.type = NEWLINE;
				running = 0;
			} break;
			case 39:
			{
				running = handle_quotes(c, &position, buffer, &token, &contains_quotes);
			} break;
			case 34:
			{
				running = handle_quotes(c, &position, buffer, &token, &contains_quotes);
			} break;
			default:
			{
				append_char(&token.word, c);
			} break;
		}

		if (buffer[position] == ' ' || buffer[position] == '\n' || buffer[position] == '\t' || buffer[position] == '|' || buffer[position] == '\0')
		{
			break;
		}
	}

	if (token.type == WORD || token.type == NAME || token.type == PARAMETER_EXPANSION) // if token.type == END then error occurred during handling quotes
	{
		append_char(&token.word, '\0');
	}
	else
	{
		free_buffer(&token.word);
	}

	scanner->position = position;

	return token;
}

int skip_delim(struct Scanner* scanner)
{
	while (scanner->buffer[scanner->position] == ' ' || scanner->buffer[scanner->position] == '\t')
	{
		scanner->position++;
	}

	return scanner->buffer[scanner->position] != '\0';
}

static void arithm_read_param_exp(struct Scanner* scanner, struct Token* token)
{
	const char* buffer = scanner->buffer;
	size_t position = scanner->position;

	init_buffer(&token->word);

	char c;
	while (1)
	{
		c = buffer[++position];

		if (c != ' ' && c != '\t' && c != '\n' && c != '\0' && c != ')')
		{
			append_char(&token->word, c);
		}
		else
		{
			break;
		}
	}

	if (c == '\0' || !token->word.size)
	{
		free_token(token);
	}
	else
	{
		token->type = PARAMETER_EXPANSION;
		append_char(&token->word, '\0');
	}

	scanner->position = position;
} 

struct Token arithm_get_next_token(struct Scanner* scanner, int* arithm_expr_end)
{
	struct Token token;
	token.type = END;

	if (!skip_delim(scanner))
	{
		return token;
	}

	char c = scanner->buffer[scanner->position];

	switch (c)
	{
		case '+':
		{
			token.type = PLUS;
			scanner->position++;
		} break;
		case '-':
		{
			token.type = MINUS;
			scanner->position++;
		} break;
		case '*':
		{
			token.type = MULTIPLY;
			scanner->position++;
		} break;
		case '/':
		{
			token.type = DIVIDE;
			scanner->position++;
		} break;
		case '(':
		{
			token.type = LPAR;
			scanner->position++;
		} break;
		case ')':
		{
			token.type = RPAR;
			scanner->position++;
		} break;
		case '$':
		{
			arithm_read_param_exp(scanner, &token);
		} break;
		default:
		{
			arithm_read_integer(scanner, &token);
		} break;
	}

	return token;
}

void arithm_read_integer(struct Scanner* scanner, struct Token* token)
{
	const char* buffer = scanner->buffer;
	size_t position = scanner->position;

	init_buffer(&token->word);

	for (char c; (c = buffer[position]) != '\0'; position++)
	{
		if (c >= '0' && c <= '9')
		{
			append_char(&token->word, c);
		}
		else
		{
			break;
		}
	}

	if (token->word.size && buffer[position] != '\0')
	{
		append_char(&token->word, '\0');
		token->type = INTEGER;
	}
	else
	{
		free_token(token);
	}

	scanner->position = position;
}

void copy_token(struct Token* dest, struct Token* src)
{
	dest->type = src->type;
	dest->word.buffer = src->word.buffer;
	dest->word.capacity = src->word.capacity;
	dest->word.size = src->word.size;
}