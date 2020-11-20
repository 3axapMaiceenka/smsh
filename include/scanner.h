#ifndef SACNNER_H
#define SCANNER_H

#include <stdlib.h>

#define BUF_CAP 32

enum TokenType
{
	NEWLINE,
	WORD,
	NAME,
	PARAMETER_EXPANSION, // '$...'
	INPUT_REDIRECT, // '<'
	OUTPUT_REDIRECT, // '>'
	PIPE, // '|'
	ASYNC_LIST, // '&'
	SEQ_LIST, // ';'
	IF,
	ELSE,
	FI,
	THEN,
	FOR,
	WHILE,
	DO,
	DONE,
	IN,

	/*these are used while reading an arithmetic expression*/
	INTEGER,
	PLUS,
	MINUS,
	MULTIPLY,
	DIVIDE,
	LPAR,
	RPAR,
	/****************************************************/

	END
};

struct Scanner
{
	char* buffer;
	size_t position;
};

struct Buffer
{
	char* buffer;
	size_t capacity;
	size_t size;
};

struct Token
{
	struct Buffer word;
	enum TokenType type;
};

void init_buffer(struct Buffer* buffer);

void append_char(struct Buffer* buffer, char c);

struct Token get_next_token(struct Scanner* scanner, int* arithm_expr_beginning);

struct Token arithm_get_next_token(struct Scanner* scanner, int* arithm_expr_end);

void arithm_read_integer(struct Scanner* scanner, struct Token* token);

void copy_token(struct Token* dest, struct Token* src);

int skip_delim(struct Scanner* scanner);

// reads symbols from buffer from position until encountered terminating quote(returns 1 in that case) or '\0'(returns 0)
int handle_quotes(char quote, size_t* position, const char* buffer, struct Token* token, int* contains_quotes);

int handle_io_redirect(char redirect, const char* buffer, size_t* position, int contains_quotes, struct Token* token);

#endif