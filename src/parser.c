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

static struct AstNode* create_ast_node(void* actual_data, enum AstNodeType node_type)
{
	struct AstNode* node = malloc(sizeof(struct AstNode));
	node->actual_data = actual_data;
	node->node_type = node_type;
	return node;
}

/*
cmd_prefix :            NAME'='WORD
		   |            NAME'='PARAMETER_EXPANSION
		   |            NAME'='arithm_expression
		   | cmd_prefix NAME'='WORD
		   | cmd_prefix NAME'='PARAMETER_EXPANSION
		   | cmd_prefix NAME'='arithm_expression
*/
AssignmentsList* cmd_prefix(struct Parser* parser)
{
	if (parser->current_token.type != NAME)
	{
		return NULL;
	}

	AssignmentsList* assignment_list = create_list(&free_ast_assignment);
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
				node = create_ast_node(ast_word(parser), AST_WORD);
			}
		}

		if (node)
		{
			assignment = calloc(1, sizeof(struct AstAssignment));
			assignment->variable = malloc(sizeof(struct Token));
			copy_token(assignment->variable, &token);
			assignment->expression = node;
			push_back(assignment_list, assignment);
		}
		else
		{
			destroy_list(&assignment_list);
			free(assignment_list);

			if (!parser->error->error)
			{
				set_error(parser->error, "invalid assignment statement");
			}

			return NULL;
		}
	} while (parser->current_token.type == NAME);

	return assignment_list;
}

// filenme: WORD
//        | PARAMETER_EXPANSION
struct AstWord* filename(struct Parser* parser)
{
	return ast_word(parser);
}

/*
io_redirect : INPUT_REDIRECT  filename
			| OUTPUT_REDIRECT filename
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
			set_error(parser->error, "incomplete I/O redirection! Expected filename!");
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
		set_error(parser->error, "invalid arithmetic expression!");
		return NULL;
	}

	struct AstNode* node = create_ast_node(arithm_expr, AST_ARITHM_EXPR);

	parser->parsing_arithm_expr = 0;
	parser->current_token = get_next_token(parser->scanner, &parser->parsing_arithm_expr);

	return node;
}

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
					node = create_ast_node(arg, AST_WORD);
				}				
			}

			if (node)
			{
				if (!cmd_args->command_args)
				{
					cmd_args->command_args = create_list(&free_ast_node);
				}

				push_back(cmd_args->command_args, node);
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

// cmd_name : WORD
//          | PARAMETER_EXPANSION
struct AstWord* cmd_name(struct Parser* parser)
{
	return ast_word(parser);
}

/*
simple_command : cmd_prefix cmd_name cmd_suffix
			   | cmd_prefix cmd_name
			   | cmd_prefix
			   |            cmd_name cmd_suffix
			   |            cmd_name
*/
struct AstSimpleCommand* simple_command(struct Parser* parser)
{
	struct AstSimpleCommand* ast_scommand = NULL;

	AssignmentsList* assignment_list = cmd_prefix(parser);
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
arithm_expression : arithm_term
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
arithm_term : arithm_factor
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
arithm_factor : INTEGER
			  | PARAMETER_EXPANSION
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
		case PARAMETER_EXPANSION:
		{
			node = calloc(1, sizeof(struct AstArithmExpr));
			node->token = parser->current_token;
			eat(parser, PARAMETER_EXPANSION, arithm_get_next_token);
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
		} break;
	}

	return node;
}

/*
pipeline :                        simple_command
		 | pipeline '|' linebreak simple_command
*/
struct AstPipeline* pipeline(struct Parser* parser)
{
	struct AstSimpleCommand* ast_scommand = simple_command(parser);
	if (!ast_scommand)
	{
		return NULL;
	}

	struct AstPipeline* ast_pipeline = calloc(1, sizeof(struct AstPipeline));
	ast_pipeline->pipeline = create_list(free_ast_simple_command);
	push_back(ast_pipeline->pipeline, ast_scommand);

	while (parser->current_token.type == PIPE)
	{
		eat(parser, PIPE, get_next_token);
		linebreak(parser);

		if ((ast_scommand = simple_command(parser)) != NULL)
		{
			push_back(ast_pipeline->pipeline, ast_scommand);
		}
		else
		{
			free_ast_pipeline(ast_pipeline);
			set_error(parser->error, "incomplete pipeline!");
			return NULL;
		}
	}

	return ast_pipeline;
}

/*
linebreak : newline_list
		  | 'empty'
*/
int linebreak(struct Parser* parser)
{
	if (parser->current_token.type == NEWLINE)
	{
		return newline_list(parser);
	}

	return 1;
}

/*
newline_list :              NEWLINE
			 | newline_list NEWLINE
*/
int newline_list(struct Parser* parser)
{
	if (parser->current_token.type != NEWLINE)
	{
		return 0;
	}

	do
	{
		eat(parser, NEWLINE, get_next_token);
	} while (parser->current_token.type == NEWLINE);

	return 1;
}

/*
list : newline_list pipeline_list
	 |              pipeline_list
*/
PipelinesList* list(struct Parser* parser)
{
	if (parser->current_token.type == NEWLINE)
	{
		newline_list(parser);
	}

	PipelinesList* pipe_list = pipeline_list(parser);
	if (!pipe_list)
	{
		return NULL;
	}

	return pipe_list;
}

/*
pipeline_list : pipeline_list separator pipeline
			  | pipeline_list separator
			  |                         pipeline
*/
PipelinesList* pipeline_list(struct Parser* parser)
{
	struct AstPipeline* pipe = pipeline(parser);
	if (!pipe)
	{
		return NULL;
	}

	PipelinesList* pipe_list = create_list(free_ast_pipeline);
	push_back(pipe_list, pipe);

	while (parser->current_token.type == ASYNC_LIST || parser->current_token.type == SEQ_LIST || parser->current_token.type == NEWLINE)
	{
		pipe->mode = separator(parser);

		pipe = pipeline(parser);
		if (pipe)
		{
			push_back(pipe_list, pipe);
		}
		else
		{
			break;
		}
	}

	return pipe_list;
}

/*
separator : separator_op linebreak
		  | newline_list
*/
enum RunningMode separator(struct Parser* parser)
{
	enum RunningMode mode = FOREGROUND;

	if (parser->current_token.type == ASYNC_LIST || parser->current_token.type == SEQ_LIST)
	{
		mode = separator_op(parser);
		linebreak(parser);
	}
	else
	{
		if (!newline_list(parser))
		{
			mode = ERROR;
		}
	}

	return mode;
}

/*
separator_op : ASYNC_LIST
			 | SEQ_LIST
*/
enum RunningMode separator_op(struct Parser* parser) 
{
	switch (parser->current_token.type)
	{
		case ASYNC_LIST:
		{
			eat(parser, ASYNC_LIST, get_next_token);
			return BACKGROUND;
		} break;
		case SEQ_LIST:
		{
			eat(parser, SEQ_LIST, get_next_token);
			return FOREGROUND;
		} break;
		default:
		{
			set_error(parser->error, "expected '&' or ';' token");
			return ERROR;
		} break;
	}
}

/*
compound_list :              term
			  | newline_list term
			  |              term newline_list
			  | newline_list term newline_list
*/
CommandsList* compoud_list(struct Parser* parser)
{
	newline_list(parser); // just returns 0 if no newlines

	CommandsList* commands = term(parser);

	if (commands)
	{
		newline_list(parser);
	}

	return commands;
}

/*
term : cc_list term
	 | list    term
	 | list
	 | cc_list
*/
CommandsList* term(struct Parser* parser)
{
	CommandsList* commands_list = create_list(free_ast_node);
	CompoundCommandsList* compound_commands = NULL;
	PipelinesList* pipes = NULL;

	do
	{
		pipes = list(parser);

		if (pipes)
		{
			push_back(commands_list, create_ast_node(pipes, AST_PIPELINE_LIST));
		}

		compound_commands = cc_list(parser);

		if (compound_commands)
		{
			push_back(commands_list, create_ast_node(compound_commands, AST_COMPOUND_COMMANDS_LIST));
		}
	} while (compound_commands);

	if (!commands_list->head)
	{
		destroy_list(&commands_list);
	}

	return commands_list;
}

/*
cc_list : cc_list newline_list compound_command
		|                      compound_command
		|         newline_list compound_command
*/
CompoundCommandsList* cc_list(struct Parser* parser)
{
	newline_list(parser); // returns 0 if no newlines

	struct AstNode* node = compound_command(parser);
	if (!node)
	{
		return NULL;
	}

	CompoundCommandsList* commands_list = create_list(free_ast_node);
	push_back(commands_list, node);

	while (1)
	{
		newline_list(parser);
		node = compound_command(parser);

		if (node)
		{
			push_back(commands_list, node);
		}
		else
		{
			break;
		}	
	}

	return commands_list;
}

/*
compound_command: for_clause
				| if_clause
				| while_clause
For now:
compouns_command: if_caluse
				| while_clause
*/
struct AstNode* compound_command(struct Parser* parser)
{
	//TODO: add for_clause

	struct AstNode* node = if_clause(parser);

	if (!node)
	{
		return while_clause(parser);
	}

	return node;
}

/*
if_clause : If compound_list Then compound_list else_part Fi
		  | If compound_list Then compound_list           Fi
*/
struct AstNode* if_clause(struct Parser* parser)
{
	if (!eat(parser, IF, get_next_token))
	{
		return NULL;
	}

	struct AstIf* ast_if = calloc(1, sizeof(struct AstIf));
	ast_if->condition = compoud_list(parser);

	if (!ast_if->condition)
	{
		free(ast_if);
		set_error(parser->error, "invalid 'if' statement!");
		return NULL;
	}

	if (!eat(parser, THEN, get_next_token))
	{
		destroy_list(&ast_if->condition);
		free(ast_if);
		set_error(parser->error, "expected 'then' token in 'if' statement!");
		return NULL;
	}

	ast_if->if_part = compoud_list(parser);

	if (!ast_if->if_part && parser->error->error)
	{
		destroy_list(&ast_if->condition);
		free(ast_if);
		return NULL;
	}

	ast_if->else_part = else_part(parser);
	
	if (parser->error->error)
	{
		destroy_list(&ast_if->if_part);
		destroy_list(&ast_if->condition);
		free(ast_if);
		return NULL;
	}

	if (!eat(parser, FI, get_next_token))
	{
		destroy_list(&ast_if->condition);
		destroy_list(&ast_if->if_part);	
		destroy_list(&ast_if->else_part);
		free(ast_if);
		set_error(parser->error, "expected 'fi' token in 'if' statement!");
		return NULL;
	}

	return create_ast_node(ast_if, AST_IF);
}

//else_part: Else compound_list
CommandsList* else_part(struct Parser* parser)
{
	if (!eat(parser, ELSE, get_next_token))
	{
		return NULL;
	}

	return compoud_list(parser);
}

//while_clause: While compound_list do_group
struct AstNode* while_clause(struct Parser* parser)
{
	if (!eat(parser, WHILE, get_next_token))
	{
		return NULL;
	}

	CommandsList* condition = compoud_list(parser);
	if (!condition)
	{
		set_error(parser->error, "invalid 'while' loop!");
		return NULL;
	}

	CommandsList* body = do_group(parser);
	if (!body && parser->error->error)
	{
		destroy_list(&condition);
		return NULL;
	}

	struct AstWhile* ast_while = malloc(sizeof(struct AstWhile));
	ast_while->body = body;
	ast_while->condition = condition;

	return create_ast_node(ast_while, AST_WHILE);
}

//do_group : Do compound_list Done
CommandsList* do_group(struct Parser* parser)
{
	if (!eat(parser, DO, get_next_token))
	{
		set_error(parser->error, "expected 'do' token!");
		return NULL;
	}

	CommandsList* commands = compoud_list(parser);

	if (parser->error->error)
	{
		destroy_list(&commands);
		return NULL;
	}
	
	if (!eat(parser, DONE, get_next_token))
	{
		set_error(parser->error, "expected 'done' token!");
		destroy_list(&commands);
		return NULL;
	}

	return commands;
}

CommandsList* parse(struct Parser* parser)
{
	if (parser->current_token.type == END)
	{
		return NULL;
	}

	CommandsList* program = compoud_list(parser);

	//newline_list(parser); ????

	if (!parser->error->error && parser->current_token.type != END)
	{
		set_error(parser->error, "invalid token!");
	}

	return program;
}

void free_ast_simple_command(void* ast_scommand)
{
	if (ast_scommand)
	{
		struct AstSimpleCommand* command = (struct AstSimpleCommand*)ast_scommand;
		destroy_list(&command->assignment_list);
		destroy_list(&command->command_args);
		free_ast_word(command->command_name);
		free_ast_io_redirect(command->input_redirect);
		free_ast_io_redirect(command->output_redirect);
		free(command);
	}
}

void free_ast_assignment(void* assignment)
{
	if (assignment)
	{
		struct AstAssignment* a = (struct AstAssignment*)assignment;
		free_ast_node(a->expression);
		free(a->variable->word.buffer);
		free(a->variable);
		free(a);
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
			case AST_IF:
			{
				free_ast_if(n->actual_data);
			} break;
			case AST_FOR:
			{
				// free ast for
			} break;
			case AST_WHILE:
			{
				free_ast_while(n->actual_data);
			} break;
			case AST_ARITHM_EXPR:
			{
				free_ast_arithm_expr(n->actual_data);
			} break;
			case AST_PIPELINE_LIST:
			{
				destroy_list((struct List**)&n->actual_data);
			} break;
			case AST_COMPOUND_COMMANDS_LIST:
			{
				destroy_list((struct List**)&n->actual_data);
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

		if (w->word.word.buffer)
		{
			free(w->word.word.buffer);
		}

		free(w);
	}
}

void free_ast_arithm_expr(void* arithm_expr)
{
	if (arithm_expr)
	{
		struct AstArithmExpr* expr = (struct AstArithmExpr*)arithm_expr;

		if (expr->left)
		{
			free_ast_arithm_expr(expr->left);
		}
		if (expr->right)
		{
			free_ast_arithm_expr(expr->right);
		}

		if (expr->token.word.buffer)
		{
			free(expr->token.word.buffer);
		}

		free(expr);
	}
}

void free_ast_pipeline(void* ast_pipeline)
{
	if (ast_pipeline)
	{
		struct AstPipeline* pipe = (struct AstPipeline*)ast_pipeline;
		destroy_list(&pipe->pipeline);
		free(pipe);
	}
}

void free_ast_if(void* ast_if)
{
	if (ast_if)
	{
		struct AstIf* if_statement = (struct AstIf*)ast_if;
		destroy_list(&if_statement->condition);
		destroy_list(&if_statement->if_part);
		destroy_list(&if_statement->else_part);
		free(if_statement);
	}
}

void free_ast_while(void* ast_while)
{
	if (ast_while)
	{
		struct AstWhile* while_loop = (struct AstWhile*)ast_while;
		destroy_list(&while_loop->body);
		destroy_list(&while_loop->condition);
		free(while_loop);
	}
}