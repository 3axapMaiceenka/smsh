#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>
#include "scanner.h"

struct Parser
{
	struct Token current_token;
	struct Scanner* scanner;
	struct Error* error;
	int parsing_arithm_expr;
};

enum AstNodeType
{
	AST_WORD,
	AST_ARITHM_EXPR,
	AST_IF,
	AST_FOR,
	AST_WHILE,
	AST_PIPELINE_LIST,
	AST_COMPOUND_COMMANDS_LIST
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

/*
	Wordlist             contains AstNode structures,
	AssignmentsList      contains AstAssignment structures,
	PipelinesList        contains AstPipelineStructures ,
	SimpleCommndsList    contains AstSimpleCommandStructures,
	CompoundCommandsList contains AstNode structures,
	CommandsList         contains AstNode structures
	See create_list() and destroy_list() functions in list.h
*/
typedef struct List Wordlist, AssignmentsList, PipelinesList, SimpleCommandsList, CompoundCommandsList, CommandsList;

struct AstAssignment
{
	struct Token* variable; // variable->type == NAME
	struct AstNode* expression; // expresision->node_type is AST_WORD or AST_ARITHM_EXPR
};

struct AstIORedirect
{
	struct Token* token; // token->type == INPUT_REDIRECT or token->type == OUTPUT_REDIRECT
	struct AstWord* file_name;
};

struct CmdArgs
{
	Wordlist* command_args;
	struct AstIORedirect* input_redirect;
	struct AstIORedirect* output_redirect;
};

struct AstSimpleCommand
{
	Wordlist* command_args;
	AssignmentsList* assignment_list;
	struct AstWord* command_name;
	struct AstIORedirect* input_redirect;
	struct AstIORedirect* output_redirect;
};

enum RunningMode // temp
{
	FOREGROUND,
	BACKGROUND,
	ERROR
};

struct AstPipeline
{
	SimpleCommandsList* pipeline; // list contains AstSimpleCommands structures or NULL
	enum RunningMode mode;
};

struct AstArithmExpr
{
	struct Token token;
	struct AstArithmExpr* left;
	struct AstArithmExpr* right;
};

struct AstIf
{
	CommandsList* condition;
	CommandsList* if_part;
	CommandsList* else_part; // can be NULL
};

struct AstWhile
{
	CommandsList* condition;
	CommandsList* body;
};

struct AstFor
{
	struct AstWord* variable;
	Wordlist* wordlist; // variable's values, can be NULL
	CommandsList* body;
};

/*
io_redirect : INPUT_REDIRECT  filename
            | OUTPUT_REDIRECT filename
*/
struct AstIORedirect* io_redirect(struct Parser* parser);

// filenme: WORD
//        | PARAMETER_EXPANSION
struct AstWord* filename(struct Parser* parser);

/*
cmd_suffix :	        io_redirect
           | cmd_suffix io_redirect
           |            WORD
           |            PARAMETER_EXPANSION
           |            arithm_expression
           | cmd_suffix WORD
           | cmd_suffix PARAMETER_EXPANSION
           | cmd_suffix arithm_expression
*/
struct CmdArgs* cmd_suffix(struct Parser* parser);

// cmd_name : WORD
//          | PARAMETER_EXPANSION
struct AstWord* cmd_name(struct Parser* parser);

/*
cmd_prefix :            NAME'='WORD
           |            NAME'='PARAMETER_EXPANSION
           |            NAME'='arithm_expression
           | cmd_prefix NAME'='WORD
           | cmd_prefix NAME'='PARAMETER_EXPANSION
           | cmd_prefix NAME'='arithm_expression
*/
AssignmentsList* cmd_prefix(struct Parser* parser);

/*
simple_command : cmd_prefix cmd_name cmd_suffix
               | cmd_prefix cmd_name
               | cmd_prefix
               |            cmd_name cmd_suffix
               |            cmd_name
*/
struct AstSimpleCommand* simple_command(struct Parser* parser);

/*
arithm_expression : arithm_term
                  | arithm_term (PLUS  arithm_term)*
                  | arithm_term (MINUS arithm_term)*
*/
struct AstArithmExpr* arithm_expression(struct Parser* parser);

/*
arithm_term : arithm_factor
            | arithm_factor (MULTIPLY arithm_factor)*
            | arithm_factor (DIVIDE   arithm_factor)*
*/
struct AstArithmExpr* arithm_term(struct Parser* parser);

/*
arithm_factor : INTEGER
              | PARAMETER_EXPANSION
              | LPAR arithm_expression RPAR
              | PLUS  arithm_factor
              | MINUS arithm_factor
*/
struct AstArithmExpr* arithm_factor(struct Parser* parser);

/*
pipeline :                        simple_command
         | pipeline '|' linebreak simple_command
*/
struct AstPipeline* pipeline(struct Parser* parser);

/*
linebreak : newline_list
          | 'empty'
*/
int linebreak(struct Parser* parser);

/*
newline_list :              NEWLINE
             | newline_list NEWLINE
*/
int newline_list(struct Parser* parser);

/*
list : newline_list pipeline_list
     |              pipeline_list
*/
PipelinesList* list(struct Parser* parser);

/*
pipeline_list : pipeline_list separator pipeline
              | pipeline_list separator
              |                         pipeline
*/
PipelinesList* pipeline_list(struct Parser* parser);

/*
separator : separator_op linebreak
          | newline_list
*/
enum RunningMode separator(struct Parser* parser);

/*
separator_op : ASYNC_LIST
             | SEQ_LIST
*/
enum RunningMode separator_op(struct Parser* parser); // returns RunningMode::ERROR if parser->current_token.type != SEQ_LIST && != ASYNC_LIST

/*
compound_list :              term
              | newline_list term
              |              term newline_list
              | newline_list term newline_list
*/
CommandsList* compoud_list(struct Parser* parser);

/*
term : cc_list term
     | list    term
     | list
     | cc_list
*/
CommandsList* term(struct Parser* parser);

/*
cc_list : cc_list newline_list compound_command
        |                      compound_command
        |         newline_list compound_command
*/
CompoundCommandsList* cc_list(struct Parser* parser);

/*
compound_command: for_clause
                | if_clause
                | while_clause
*/
struct AstNode* compound_command(struct Parser* parser);

/*
if_clause : If compound_list Then compound_list else_part Fi
          | If compound_list Then compound_list           Fi
*/
struct AstNode* if_clause(struct Parser* parser);

//else_part: Else compound_list
CommandsList* else_part(struct Parser* parser);

//while_clause: While compound_list do_group
struct AstNode* while_clause(struct Parser* parser);

//do_group : Do compound_list Done
CommandsList* do_group(struct Parser* parser);

//for_clause : For WORD linebreak In wordlist newline_list do_group
struct AstNode* for_clause(struct Parser* parser);

/*
wordlist : wordlist WORD
         | wordlist PARAMETER_EXPANSION
         | wordlist arithm_expression
         |          WORD
         |          PARAMETER_EXPANSION
         |          arithm_expression
*/
Wordlist* wordlist(struct Parser* parser);


CommandsList* parse(struct Parser* parser);
struct AstWord* ast_word(struct Parser* parser);

// returns result of arithm_expression and calls get_next_token()
struct AstNode* parse_arithm_expr(struct Parser* parser);

// get_token function is get_next_token or arithm_get_next_token
int eat(struct Parser* parser, enum TokenType expected, struct Token(*get_token)(struct Scanner*, int*));

void free_ast_simple_command(void* ast_scommand);
void free_ast_io_redirect(void* io_redir);
void free_ast_word(void* word);
void free_ast_assignment(void* assignment);
void free_ast_node(void* node);
void free_ast_arithm_expr(void* arithm_expr);
void free_ast_pipeline(void* ast_pipeline);
void free_ast_if(void* ast_if);
void free_ast_while(void* ast_while);
void free_ast_for(void* ast_for);

#endif