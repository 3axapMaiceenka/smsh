#include "parser.h"
#include "list.h"
#include "utility.h"
#include <stdio.h>

// get_token(...) function is get_next_token(...) or arithm_get_next_token(...) (if parser->parsing_arithm_expr == 1)
int eat(struct Parser* parser, enum TokenType expected, struct Token(*get_token)(struct Scanner*, int*))
{
	if (parser->current_token.type == expected)
	{
		// scanner sets parser->parsing_arithm_expr variable to 1, if encounter '$((' sequence 
		parser->current_token = get_token(parser->scanner, &parser->parsing_arithm_expr); 
		return 1;
	}

	return 0;
}

struct AstWord* ast_word(struct Parser* parser)
{
	if (parser->current_token.type != WORD && parser->current_token.type != PARAMETER_EXPANSION)
	{
		return NULL;
	}

	struct AstWord* word = malloc(sizeof(struct AstWord));
	copy_token(&word->word, &parser->current_token);
	eat(parser, word->word.type, get_next_token);

	return word;
}

/*
cmd_prefix:				  NAME'='WORD
			 |			  NAME'='PARAMETER_EXPANSION
			 |			  NAME'='arithm_expression
			 | cmd_prefix NAME'='WORD
			 | cmd_prefix NAME'='PARAMETER_EXPANSION
			 | cmd_prefix NAME'='arithm_expression
*/
struct AstAssignmentList* cmd_prefix(struct Parser* parser)
{
	if (parser->current_token.type != NAME)
	{
		return NULL;
	}

	struct AstAssignmentList* assignment_list = calloc(1, sizeof(struct AstAssignmentList));
	assignment_list->assignments = create_list(&free_ast_assignment);

	struct AstAssignment* assignment = NULL;
	struct Token token;

	do
	{
		copy_token(&token, &parser->current_token);
		eat(parser, NAME, get_next_token);

		struct AstNode* node = NULL;

		if (parser->parsing_arithm_expr)
		{
			node = parse_arithm_expr(parser); // parse_arithm_expr() returns result of arithm_expression() and calls get_next_token()
		}
		else
		{
			if (parser->current_token.type == WORD || parser->current_token.type == PARAMETER_EXPANSION)
			{
				node = malloc(sizeof(struct AstNode));
				node->node_type = AST_WORD;
				node->actual_data = ast_word(parser); // calls eat()
			}
		}

		if (node)
		{
			assignment = calloc(1, sizeof(struct AstAssignment));
			assignment->variable = malloc(sizeof(struct Token));
			copy_token(assignment->variable, &token);
			assignment->expression = node;
			push_back(assignment_list->assignments, assignment);
		}
		else
		{
			destroy_list(&assignment_list->assignments);
			free(assignment_list);

			if (!parser->error->error)
			{
				set_error(parser->error, "Syntax error: invalid assignment statement\n");
			}

			return NULL;
		}
	} while (parser->current_token.type == NAME);

	return assignment_list;
}

// filenme:   WORD
//		    | PARAMETER_EXPANSION
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
		eat(parser, parser->current_token.type, get_next_token);

		struct AstWord* file_name = filename(parser);
		if (!file_name) 
		{
			free(token);
			set_error(parser->error, "Syntax error: incomplete I/O redirection! Expected filename!\n");
			return NULL;
		}

		struct AstIORedirect* io_redir = malloc(sizeof(struct AstIORedirect));
		io_redir->file_name = file_name;
		io_redir->token = token;

		return io_redir;
	}

	return NULL;
}

static int finish_reading_arithm_expr(struct Scanner* scanner)
{
	if (scanner->buffer[scanner->position] == ')')
	{
		scanner->position++;
		return 1;
	}

	return 0;
}

// returns result of arithm_expression
struct AstNode* parse_arithm_expr(struct Parser* parser)
{
	parser->current_token = arithm_get_next_token(parser->scanner, &parser->parsing_arithm_expr);

	struct AstArithmExpr* arithm_expr = arithm_expression(parser);
	if (!arithm_expr || !finish_reading_arithm_expr(parser->scanner))
	{
		set_error(parser->error, "Syntax error: invalid arithmetic expression!");
		return NULL;
	}

	struct AstNode* node = malloc(sizeof(struct AstNode));
	node->actual_data = arithm_expr;
	node->node_type = AST_ARITHM_EXPR;

	parser->parsing_arithm_expr = 0;
	parser->current_token = get_next_token(parser->scanner, &parser->parsing_arithm_expr);

	return node;
}

/*
cmd_suffix:				 io_redirect
			| cmd_suffix io_redirect
			|			 WORD
			|			 PARAMETER_EXPANSION
			|			 arithm_expression
			| cmd_suffix WORD
			| cmd_suffix PARAMETER_EXPANSION
			| cmd_suffix arithm_expression
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

			struct AstNode* node = NULL;

			if (parser->parsing_arithm_expr)
			{
				if (!(node = parse_arithm_expr(parser))) // parse_arithm_expr() returns result of arithm_expression() and calls get_next_token()
				{
					return cmd_args;
				}
			}
			else
			{
				struct AstWord* arg = ast_word(parser);

				if (arg)
				{
					node = malloc(sizeof(struct AstNode));
					node->actual_data = arg;
					node->node_type = AST_WORD;
				}				
			}

			if (node)
			{
				if (!cmd_args->command_args)
				{
					cmd_args->command_args = calloc(1, sizeof(struct AstWordlist));
					cmd_args->command_args->wordlist = create_list(&free_ast_node);
				}

				push_back(cmd_args->command_args->wordlist, node);
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
				|            cmd_name cmd_suffix
				|            cmd_name
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
			free(cmd_args);
		}
	}

	return ast_scommand;
}

/*
arithm_expression:   arithm_term
				   | arithm_term (PLUS  arithm_term)*
				   | arithm_term (MINUS arithm_term)*
*/
struct AstArithmExpr* arithm_expression(struct Parser* parser)
{
	struct AstArithmExpr* node = arithm_term(parser);
	if (!node)
	{
		return NULL;
	}

	while (parser->current_token.type == PLUS || parser->current_token.type == MINUS)
	{
		enum TokenType type = parser->current_token.type;
		eat(parser, type, arithm_get_next_token);

		struct AstArithmExpr* temp = calloc(1, sizeof(struct AstArithmExpr));
		temp->left = node;
		temp->token.type = type;
		temp->right = arithm_term(parser);

		node = temp;
	}

	return node;
}

/*
arithm_term:   arithm_factor
			 | arithm_factor (MULTIPLY arithm_factor)*
			 | arithm_factor (DIVIDE   arithm_factor)*
*/
struct AstArithmExpr* arithm_term(struct Parser* parser)
{
	struct AstArithmExpr* node = arithm_factor(parser);
	if (!node)
	{
		return NULL;
	}

	while (parser->current_token.type == MULTIPLY || parser->current_token.type == DIVIDE)
	{
		enum TokenType type = parser->current_token.type;
		eat(parser, type, arithm_get_next_token);

		struct AstArithmExpr* temp = calloc(1, sizeof(struct AstArithmExpr));
		temp->token.type = type;
		temp->left = node;
		temp->right = arithm_factor(parser);

		if (!temp->right)
		{
			free_ast_arithm_expr(temp);
			return NULL;
		}

		node = temp;
	}

	return node;
}

/*
arithm_factor:    INTEGER
				| LPAR arithm_expression RPAR
				| PLUS  arithm_factor
				| MINUS arithm_factor
*/
struct AstArithmExpr* arithm_factor(struct Parser* parser)
{
	struct AstArithmExpr* node = NULL;

	switch (parser->current_token.type)
	{
		case INTEGER:
		{
			node = calloc(1, sizeof(struct AstArithmExpr));
			node->token = parser->current_token;
			eat(parser, INTEGER, arithm_get_next_token);
		} break;
		case PLUS:
		{
			eat(parser, PLUS, arithm_get_next_token);
			node = calloc(1, sizeof(struct AstArithmExpr));
			node->token.type = PLUS;
			node->left = arithm_factor(parser);
		} break;
		case MINUS:
		{
			eat(parser, MINUS, arithm_get_next_token);
			node = calloc(1, sizeof(struct AstArithmExpr));
			node->token.type = MINUS;
			node->left = arithm_factor(parser);
		} break;
		case LPAR:
		{
			eat(parser, LPAR, arithm_get_next_token);
			node = arithm_expression(parser);

			if (!eat(parser, RPAR, arithm_get_next_token))
			{
				free_ast_arithm_expr(node);
				return NULL;
			}
		} break;
		default:
		{
			//TODO: handle error
		} break;
	}

	return node;
}

struct AstSimpleCommand* parse(struct Parser* parser)
{
	if (parser->current_token.type == END)
	{
		return NULL;
	}

	return simple_command(parser);
}

void set_error(struct Error* error, const char* message)
{
	error->error_message = copy_string(message);
	error->error = 1;
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

void free_ast_arithm_expr(void* arithm_expr)
{
	if (arithm_expr)
	{
		struct AstArithmExpr* a = (struct AstArithmExpr*)arithm_expr;
		
		if (a->left)
		{
			free_ast_arithm_expr(a->left);
		}
		if (a->right)
		{	
			free_ast_arithm_expr(a->right);
		}

		if (a->token.word.buffer)
		{
			free(a->token.word.buffer);
		}

		free(a);
	}
}