#include <stdio.h>

#include "cli.h"
#include "compiler.h"
#include "gl.h"
#include "parse_tree.h"
#include "exception.h"
#include "ir_compiler.h"
#include "ir_ssbo_format.h"
#include "yacc.h"
#include "lang.h"

#include "c_tokens.cpp"
#include "grammars/c_grammar.cpp"

int main(int argc, char** argv) {
    try {
        Gl::init(true);
        Job job = parse_args(argc, argv);

        auto lang_tokens_parse_tree = tokens_list_to_parse_tree(c_tokens);

        IrTokenList* ir_tokens = new IrTokenList();
        ParseTree yacc_parse_tree = ParseTree(900);
        ParseTree ir_parse_tree = ParseTree(400);
        std::vector<std::string> grammars = { c_pre_yacc, c_yacc };

        auto ast_ssbos = create_ast_ssbos(grammars, *lang_tokens_parse_tree, ir_tokens, yacc_parse_tree, ir_parse_tree);

        write_file((get_bin_dir() + "/shaders/ir_tokens_generated").c_str(),
            ir_tokens_shader_definitions(*ir_tokens).c_str());

        Shaders shaders = Gl::compile_shaders();

        compile(job, shaders, yacc_parse_tree, ir_parse_tree, lang_tokens_parse_tree, ir_tokens, ast_ssbos);
    } catch (Exception& e) {
        e.print();
        return 1;
    }
    return 0;
}