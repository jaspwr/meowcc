#include "yacc_parser.h"

const char *yacc = R"(
STRING_LITERAL
    : '"' ... '"'
    ;

CONSTANT
    : CONSTANT '.' CONSTANT
	;

unary_operation
    : { ')', expression, primary_expression, terminating_expression} $1 operator $0 primary_expression
    ; < $1.$0 == '!' : %X = xor i1 $0, true;
        $1.$0 == '~' : %X = xor $t0 $0, -1;
        $1.$0 == '-' : %X = sub nsw $t0 0, $0;
        $1.$0 == '*' : ;
        $1.$0 == '&' : ;
      >

primary_expression
	: $0 IDENTIFIER
	| $0 CONSTANT
	| $0 STRING_LITERAL
	| '(' $0 expression ')'
    | '(' $0 primary_expression ')'
    | $0 expression
    | $0 type_specifier
    | $0 function_call
    | $0 array_call
	;

expression
    : operation
    | $0 cast_expression
    ;

operation
    : $0 primary_expression $1 operator $2 primary_expression
    ; < $1.$0 == '+' : add >

two_way_branch
    : 'if' $0 primary_expression $1 basic_block $2 basic_block
    | 'if' $0 primary_expression $1 basic_block 'else' $2 basic_block
    ; < default: br i1 $0, label $1, label $2 ; >


start_connected_terminating_expression
    : '{' terminating_expression
    | $0 start_connected_terminating_expression $1 terminating_expression
    | $0 start_connected_terminating_expression $1 primary_expression expression_terminator
    ;

function
    : function_definiton basic_block_group '}'
    ;

basic_block
    : $0 start_connected_terminating_expression '}'
    | '{' '}'
    ;

cast_specifier
    : '(' $0 type_specifier ')'
    ;

cast_expression
    : $1 cast_specifier $0 primary_expression
    ;

type_specifier
	: $0 'void'
	| $0 'char'
	| $0 'short'
	| $0 'int'
	| $0 'long'
	| $0 'float'
	| $0 'double'
	| $0 'signed'
	| $0 'unsigned'
	| $0 'struct'
	| $0 'union'
	| $0 'enum'
	| $0 TYPE_NAME
	;< wee woo >

terminating_expression
    : expression expression_terminator
    | expression terminating_expression
    | primary_expression operator terminating_expression
    | assignment
    ;

assignment
    : primary_expression assignment_operator terminating_expression
    | primary_expression assignment_operator primary_expression expression_terminator 
    ;

function_call
    : function_identifier ')'
    | function_and_partial_argument_list ')'
    | function_and_partial_argument_list primary_expression ')'
    | function_identifier primary_expression ')'
    ;

array_call
    : $1 primary_expression '[' $0 primary_expression ']'
    ;

function_identifier
    : primary_expression '('
    ;

function_and_partial_argument_list
    : function_identifier primary_expression ','
    | function_and_partial_argument_list primary_expression ','
    ;

type_qualifier
	: 'const'
	| 'volatile'
	;

pointer
    : type_specifier '*'
    ;


operator
    : $0 '+'
    | $0 '*'
    | $0 '-'
    | $0 '/'
    | $0 '&'
    | $0 '~'
    | $0 '!'
    ;

assignment_operator
	: $0 '='
	| $0 '*='
	| $0 '/='
	| $0 '%='
	| $0 '+='
	| $0 '-='
	| $0 '<<='
	| $0 '>>='
	| $0 '&='
	| $0 '^='
	| $0 '|='
	;

basic_block_terminator
    : return
    | two_way_branch
    | one_way_branch
    | switch
    ;

one_way_branch
    : '}'
    ;

return
    : 'return' $0 terminating_expression
    ; < default: ret $t0 $0 ; >

expression_terminator
    : ';'
    ;


)";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum contexts{
    ab_tokens,
    _sentance,
    norm_token,
    exclusion_tokens,
    codegen
};
contexts context = contexts::ab_tokens;

int working_row = 0;
int max_row = 0;

int catergorise(char x){
    //TODO: make this not disgusting
    if(x == '\'')
        return 2;
    if(context == contexts::norm_token)
        return 6;
    if(x < '!')
        return 1;
    if(x == '|')
		return 3;
	if(x == ':')
        return 4;
	if(x == '.')
        return 5;
    if (x == '{')
        return 7;
    if (x == '}')
        return 8;
    if (x == '<')
        return 9;
    if (x == '>')
        return 10;
    if (x == ',')
        return 11;
    if (x == ';')
        return 12;
    return 0;
}





int in_sentance_index = 0;
int in_exlusion_tokens_index = 0;
int replaces_with = 0;
int parse_tree_extra_pointer = 0;
char sentance_buffer[10];
int operand_order_buffer[10];
int pre_token_exclusions_buffer[10];
contexts context_returning_to;
void new_sentace(){
    if(in_sentance_index != 0){
        char* str = (char*)malloc(in_sentance_index+1);

        parse_tree_extra new_entry_data;
        for(int i = 0; i < in_sentance_index; i++){
            new_entry_data.operand_order[i] = operand_order_buffer[i];
            str[i] = sentance_buffer[i];
        }
        for (int i = 0; i < in_exlusion_tokens_index; i++) {
            new_entry_data.pre_char_exclusions[i] = pre_token_exclusions_buffer[i];
        }
        new_entry_data.pre_char_exclusions_counter = in_exlusion_tokens_index;

        //check if identical entry in pass_tree_extra_list_exists
        bool new_entry = true;
        int entry_index;
        for (entry_index = 0;
            entry_index < parse_tree_extra_pointer;
            entry_index++) {
            if (lang::_parse_tree_extra[entry_index] == new_entry_data) {
                new_entry = false;
                break;
            }
        }
        if (new_entry) {
            lang::_parse_tree_extra[parse_tree_extra_pointer] = new_entry_data;
            entry_index = parse_tree_extra_pointer;
        }

        str[in_sentance_index] = '\0';
        add_token(str,replaces_with, entry_index);
        free(str);
        if(new_entry)
            parse_tree_extra_pointer++;
    }
    in_sentance_index = 0;
    in_exlusion_tokens_index = 0;
}

int operand_order = 0;
int operand_order_current() {
    int ret = operand_order;
    operand_order = 0;
    return ret;
}

token_tree gen_tree(token_tree* main_tt, int normal_token_count){
    lang::_parse_tree_extra = (parse_tree_extra*)malloc(sizeof(parse_tree_extra) * 255);
    flush_tree(true);
    yacc_parser::tokens.ab_token_counter = normal_token_count;
    yacc_parser::tokens.starting_ab_token_counter = yacc_parser::tokens.ab_token_counter;
    int substr_len = 1;
    int token_start = 0;
    bool white_space = true;
    #define MAX_LENGTH 10000
    for(int i = 0; i < MAX_LENGTH && yacc[i] != '\0'; i++){
        //if at least one character in token is not an empty char set to false
        if(yacc[i] > ' ' && (yacc[i] != ',' || context == contexts::codegen))
            white_space = false;

        if(catergorise(yacc[i+1]) != catergorise(yacc[i])){
            if(substr_len != 0 && !white_space){
                //run when 'i' reaches end of a token
                char *substr = (char*)malloc(substr_len + 1);
                for(int ii = 0; ii < substr_len; ii++){
                    substr[ii] = yacc[token_start + ii];
                }
                substr[substr_len] = '\0';

                // check for colon, semicolon, ect.
                bool not_marker = true;
                if(substr_len == 1)
                {
                    //TODO: reimplement this to repeat itself less
                    not_marker = false;
                    switch (substr[0])
                    {
                    case ':':
						
                        if(context != contexts::norm_token && context != contexts::codegen)
                        {
							if (context != contexts::ab_tokens)
								throw "Invalid syntax: Unexpected ':'"; // Invalid syntax
                            context = contexts::_sentance;
                            new_sentace();
                        }else{
							not_marker = true;
						}
                        break;
                    case '|':
                        if(context != contexts::norm_token && context != contexts::codegen)
                        {
                            context = contexts::_sentance;
                            new_sentace();
                        }else{
							not_marker = true;
						}
                        break;
                    case ';':
                        if(context != contexts::norm_token && context != contexts::codegen)
						{
                            context = contexts::ab_tokens;
							new_sentace();
						}else{
							not_marker = true;
						}
                        break;
                    case '\'':
                        //escape to sentance or begin new 'in quotes' block
                        if(context == contexts::norm_token)
                            context = context_returning_to;
                        else {
                            context_returning_to = context;
                            context = contexts::norm_token;
                        }
                        break;
                    case '{':
                        if (context != contexts::norm_token && context != contexts::codegen)
                            context = contexts::exclusion_tokens;
                        else
                            not_marker = true;
                        break;
                    case '}':
                        if (context != contexts::norm_token && context != contexts::codegen)
                            context = contexts::_sentance;
                        else
                            not_marker = true;
                        break;
                    case '<':
                        if (context != contexts::norm_token && context != contexts::codegen)
                            context = contexts::codegen;
                        else
                            not_marker = true;
                        break;
                    case '>':
                        if (context != contexts::norm_token){
                            context = contexts::ab_tokens;
                            ir_codegen::finalise_token(replaces_with);
                        }
                        else
                            not_marker = true;
                        break;
                    default:
                        not_marker = true;
                        break;
                    }
                }
                if (not_marker) {
                    if (context == contexts::codegen 
                        || (context == contexts::norm_token 
                            && context_returning_to == contexts::codegen)) {
                        ir_codegen::process_token(substr, context == contexts::norm_token, main_tt);
                    }
                    else {
                        //printf("%s\n", substr);
                        //Handle special tokens (TODO: implement this a neater way)
                        //
                        //      "..." -> ANYTHING_TOKEN
                        //      "IDENTIFIER" -> ANY_IDENTIFIER_TOKEN
                        //      "CONSTANT" -> ANY_LITERAL_TOKEN
                        //      
                        //      $X puts the following token as position X in the child nodes when 
                        //      constructing the ast.
                        //
                        if (simple_token_parser::str_match(substr, (char*)"...")) {
                            sentance_buffer[in_sentance_index] = ANYTHING_TOKEN;
                            operand_order_buffer[in_sentance_index] = operand_order_current();
                            in_sentance_index++;
                        }
                        else if (substr[0] == '$') {
                            //TODO implement with proper int parse function
                            operand_order = (int)(substr[1] - '0') + 1;
                        }
                        else if (simple_token_parser::str_match(substr, (char*)"IDENTIFIER")) {
                            sentance_buffer[in_sentance_index] = ANY_IDENTIFIER_TOKEN;
                            operand_order_buffer[in_sentance_index] = operand_order_current();
                            in_sentance_index++;
                        }
                        else if (simple_token_parser::str_match(substr, (char*)"CONSTANT")) {
                            sentance_buffer[in_sentance_index] = ANY_LITERAL_TOKEN;
                            operand_order_buffer[in_sentance_index] = operand_order_current();
                            in_sentance_index++;
                        }
                        else {
                            int token = 0;
                            switch (context)
                            {
                            case contexts::ab_tokens:
                                token = yacc_parser::tokens.get_ab_token_index(substr);
                                replaces_with = token;
                                //printf("%i -> ", token);
                                //printf("%s\n", substr);
                                break;
                            case contexts::_sentance:
                                token = yacc_parser::tokens.get_ab_token_index(substr);
                                sentance_buffer[in_sentance_index] = (char)token;
                                operand_order_buffer[in_sentance_index] = operand_order_current();
                                in_sentance_index++;
                                break;
                            case contexts::exclusion_tokens:
                                token = yacc_parser::tokens.get_ab_token_index(substr);
                                pre_token_exclusions_buffer[in_exlusion_tokens_index] = (int)token;
                                in_exlusion_tokens_index++;
                                break;
                            case contexts::norm_token:
                                token = parse_token_with_tree(main_tt, substr);
                                if (token == 0)
                                    throw "Unkown token in YACC"; // Token did not parse
                                //printf("%i\n", token);
                                unsigned char _ret = token;
                                if (context_returning_to == contexts::exclusion_tokens) {
                                    pre_token_exclusions_buffer[in_exlusion_tokens_index] = token;
                                    in_exlusion_tokens_index++;
                                }
                                else {
                                    sentance_buffer[in_sentance_index] = _ret; // Note: Implicite cast from uchar to char 
                                                                               //       which may cause issues.
                                    operand_order_buffer[in_sentance_index] = operand_order_current();

                                    in_sentance_index++;
                                }
                                break;
                            }
                        }
                    }
                }
                free(substr);
            }

            substr_len = 1;
            token_start = i+1;
            white_space = true;
        }else{
            substr_len++;
        }
    }
    #undef MAX_LENGTH
    

	return token_tree_gen();
}