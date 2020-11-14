#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>

#define STRING_SIZE 256

enum TokenType
{
	NEWLINE,
	WORD,
	NAME,
	ASSIGNMENT_WORD,
	PARAMETER_EXPANSION,
	ARITHMETIC_EXPANSION,
	QUOTED_STR,
	END
};

struct Token
{
	enum TokenType type;
	struct Token* nodes;
	struct Token* next;
	size_t word_start;
	size_t word_end;
};

struct Scanner
{
	char* buffer;
	size_t position;
};

struct Parser
{
	struct Token current_token;
	struct Scanner scanner;
};

enum AstNodeType
{
	AST_WORD,
	AST_WORDLIST,
	AST_SIMPLE_COMMAND
};

struct AstNode
{
	enum AstNodeType node_type;
	void* actual_data;
};

struct AstWord
{
	struct Token word;
};

struct AstWordlist
{
	struct List* wordlist;
};

struct AstSimpleCommand
{
	struct AstWord* command_name;
	struct AstWordlist* args;
};

int eat(struct Parser* parser, enum TokenType expected);
void copy_token(struct Token* dest, struct Token* src);

/*
cmd_suffix: WORD
			| cmd_suffix WORD
*/
struct AstWordlist* cmd_suffix(struct Parser* parser);

// cmd_name: WORD
struct AstWord* cmd_name(struct Parser* parser);

/*
simple_command: cmd_name cmd_suffix
				| cmd_name
*/
struct AstSimpleCommand* simple_command(struct Parser* parser);

struct AstWord* ast_word(struct Parser* parser);

struct AstSimpleCommand* parse(struct Parser* parser);
char* expand_quoted_str(struct Token* token, const char* buffer);
char* copy_memory(char* buffer, size_t* buf_size, size_t pos, const char* src, size_t src_size);
char* expand_word(struct Token* token, const char* buffer);
char* expand_token(struct Token* token, const char* buffer);
void execute(struct AstSimpleCommand* command, const char* buffer);
void destroy_token(struct Token* token);
int quoted_string(struct Scanner* scanner, struct Token* parent, struct Token* token);
int read_quoted_string(struct Scanner* scanner, struct Token* token);
void emplace_token(struct Token* token, struct Token* node);
struct Token get_next_token(struct Scanner* scanner);
void initialize(struct Parser* parser, char* buffer);
int skip_delim(struct Scanner* scanner);

#endif