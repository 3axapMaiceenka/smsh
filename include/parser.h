#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>

#define BUF_CAP 128

enum TokenType
{
	NEWLINE,
	WORD,
	NAME,
	PARAMETER_EXPANSION,
	INPUT_REDIRECT,
	OUTPUT_REDIRECT,
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

struct Error
{
	char* error_message;
	int error; // 1 or 0
};

struct Parser
{
	struct Token current_token;
	struct Scanner* scanner;
	struct Error* error;
};

enum AstNodeType
{
	AST_WORD,
	AST_WORDLIST,
	AST_SIMPLE_COMMAND,
	AST_ASSIGNMENT
};

struct AstNode
{
	enum AstNodeType node_type;
	void* actual_data;
};

struct AstWord
{
	struct Token word; // word.type is WORD or PARAMETER_EXPANSION
};

struct AstWordlist
{
	struct List* wordlist; // list contains AstWord structures or NULL
};

struct AstAssignment
{
	struct Token* variable; // variable->type == NAME
	struct AstNode* expression; // expresision->node_type is AstWord or AstArithmExpansion
};

struct AstAssignmentList
{
	struct List* assignments; // list contains AstAssigment structures or NULL
};

struct AstIORedirect
{
	struct Token* token; // token->type == INPUT_REDIRECT or token->type == OUTPUT_REDIRECT
	struct AstWord* file_name;
};

struct CmdArgs
{
	struct AstWordlist* command_args;
	struct AstIORedirect* input_redirect;
	struct AstIORedirect* output_redirect;
};

struct AstSimpleCommand
{
	struct AstWord* command_name;
	struct AstWordlist* command_args;
	struct AstIORedirect* input_redirect;
	struct AstIORedirect* output_redirect;
	struct AstAssignmentList* assignment_list;
};

/*
io_redirect:   '<' filename
			 | '>' filename
*/
struct AstIORedirect* io_redirect(struct Parser* parser);

// filenme: WORD
//        | PARAMETER_EXPANSION
struct AstWord* filename(struct Parser* parser);

/*
cmd_suffix:				 io_redirect
			| cmd_suffix io_redirect
			| WORD
			| PARAMETER_EXPANSION
			| cmd_suffix WORD
			| cmd_suffix PARAMETER_EXPANSION
*/
struct CmdArgs* cmd_suffix(struct Parser* parser);

// cmd_name:   WORD
//			 | PARAMETER_EXPANSION
struct AstWord* cmd_name(struct Parser* parser);

/*
cmd_prefix:    NAME'='WORD
			 | NAME'='PARAMETER_EXPANSION
             | cmd_prefix NAME'='WORD
			 | cmd_prefix NAME'='PARAMETER_EXPANSION
*/			    
struct AstAssignmentList* cmd_prefix(struct Parser* parser);

/*
simple_command:   cmd_prefix cmd_name cmd_suffix
				| cmd_prefix cmd_name
				| cmd_prefix
				|            cmd_name cmd_suffix
				|            cmd_name
*/
struct AstSimpleCommand* simple_command(struct Parser* parser);

struct AstWord* ast_word(struct Parser* parser);

/*
pipeline:                         simple_command
	     | pipeline '|' linebreak simple_command

struct AstPipeline* pipeline(struct Parser* parser);

linebreak: newline_list
	        | 'empty'

int linebreak(struct Parser* parser);


	newline_list :              NEWLINE
	             | newline_list NEWLINE
int newline_list(struct Parser* parser);
*/

struct AstSimpleCommand* parse(struct Parser* parser);
void set_error(struct Error* error, const char* message);

void free_ast_simple_command(void* ast_scommand); // ast_scommand is a pointer to struct AstSimpleCommand
void free_ast_assignment_list(void* assignment_list); // assignment_list is a pointer to struct AstAssignmentList
void free_ast_io_redirect(void* io_redir); // io_redir is a pointer to struct AstIORedirect
void free_ast_word(void* word); // word is a pointer to struct AstWord
void free_ast_wordlist(void* wordlist); // wordlist is a pointer to struct AstWordlist
void free_ast_assignment(void* assignment); // assignment is a pointer to struct AstAssignment
void free_ast_node(void* node); // node is a pointer to struct AstNode

int eat(struct Parser* parser, enum TokenType expected);

void init_buffer(struct Buffer* buffer);

void append_char(struct Buffer* buffer, char c);

/*scanner part*/

struct Token get_next_token(struct Scanner* scanner);

void copy_token(struct Token* dest, struct Token* src);

int skip_delim(struct Scanner* scanner);

// reads symbols from buffer from position until encountered terminating quote(returns 1 in that case) or '\0'(returns 0)
int handle_quotes(char quote, size_t* position, const char* buffer, struct Token* token, int* contains_quotes);

int handle_io_redirect(char redirect, const char* buffer, size_t* position, int contains_quotes, struct Token* token);

#endif