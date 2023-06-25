#include "compiler.h"
#include "utils.h"
#include "gl.h"
#include "shader.h"
#include "lang.h"
#include "yacc.h"
#include "ir_compiler.h"
#include "debug_printing.h"
#include "shader_structures.h"
#include "gl_atomic_counter.h"
#include "exception.h"
#include "preprocessor.h"
#include "postprocessor.h"
#include "instruction_selection_syntax.h"

#include "type_propagation.h"
#include "x86_64_selection.h"

#include "include/glad/glad.h"
#include <math.h>
#include <iostream>

//#define BENCHMARKING

#ifdef BENCHMARKING
#include "benchmark.h"
#endif

Ssbo* tokenise(UintString& source, Shader* tokeniser) {
    #ifdef BENCHMARKING
    auto bm = Benchmark("Tokeniser SSBO Setup");
    #endif


    Ssbo* tokens = new Ssbo(source.length * sizeof(Token));
    tokens->bind(2);

    #ifdef BENCHMARKING
    bm.finalise();
    #endif

    #ifdef BENCHMARKING
    auto bm_ = Benchmark("Tokeniser Exec");
    #endif

    tokeniser->exec(ceil((source.length / TOKENISER_INVOCATIONS) / TOKENISER_CHARS_PER_INV)+1);

    #ifdef BENCHMARKING
    bm_.finalise();
    #endif

    return tokens;
}

GLuint fetch_entry_node_volume(Ssbo* tokens, AstNode* ast_nodes_dmp) {
    #ifdef BENCHMARKING
    auto bm = Benchmark("Out volume fetch");
    #endif

    Token* tokens_dmp = (Token*)tokens->dump();
    GLuint ast_node_location = tokens_dmp[0].ast_node_location;
    delete[] tokens_dmp;
    GLuint volume = 0;
    AstNode entry_node = ast_nodes_dmp[ast_node_location - 1];
    for (auto child : entry_node.children) {
        volume += child.codegenVolume;
    }
    return volume;

    #ifdef BENCHMARKING
    bm.finalise();
    #endif
}

std::string load_source(std::vector<std::string> source_files, VariableRegistry& var_reg, Job& job) {
    #ifdef BENCHMARKING
    auto bm = Benchmark("Preprocessing");
    #endif

    // TODO: separate files appropriately
    std::string source = std::string();
    for (std::string _source : source_files) {
        source += preprocess(_source, var_reg);
    }
    if (job.dbg) printf("PREPROC OUT:\n%s\n", source.c_str());

    #ifdef BENCHMARKING
    bm.finalise();
    #endif
    return source;
}

void codegen_shdr_exec(Shaders& shaders, GLuint output_buffer_size) {
    #ifdef BENCHMARKING
    auto bm = Benchmark("codegen Exec");
    #endif

    shaders.codegen.exec((output_buffer_size / CODEGEN_INVOCATIONS) + 1);

    #ifdef BENCHMARKING
    bm.finalise();
    #endif
}

#define MAX_AST_NODES 100
#define AST_NODES_OVERFLOW_BUFFER_SIZE 50

std::string compile(Job& job, Shaders& shaders, ParseTree& yacc_parse_tree,
                    ParseTree& ir_parse_tree, ParseTree* lang_tokens_parse_tree,
                    IrTokenList* ir_tokens, ast_ssbos _ast_ssbos) {

    #ifdef BENCHMARKING
    auto all = Benchmark("All");
    #endif

    if (job.source_files.size() == 0) { throw Exception("No source files specified."); }

    VariableRegistry var_reg = VariableRegistry();

    std::string source_str = load_source(job.source_files, var_reg, job);
    auto source = to_uint_string(source_str);


    auto ast_nodes = Ssbo((AST_NODES_OVERFLOW_BUFFER_SIZE + source.length) * sizeof(AstNode));

    var_reg.new_register_next = AST_NODES_OVERFLOW_BUFFER_SIZE + source.length + 20;


    auto pt_ssbo = lang_tokens_parse_tree->into_ssbo();
    pt_ssbo->bind(0);

    auto ast_overflow_atomic_counter = GLAtomicCounter(0);
    ast_overflow_atomic_counter.bind(0);

    Ssbo source_ssbo = Ssbo(source.length * sizeof(GLuint), source.data);
    source_ssbo.bind(5);

    auto tokens = tokenise(source, &shaders.tokeniser);

    const GLuint ast_invocations_count = (tokens->size / AST_INVOCATIONS) / AST_CHARS_PER_INV + 1;


    if (job.dbg) tokens->print_contents();


    // IDK what the fuck is going on here
    // tokens->dump();

    #ifdef BENCHMARKING
    auto bm_gram = Benchmark("Grammar SSBO Setup");
    #endif


    #ifdef BENCHMARKING
    bm_gram.finalise();
    #endif


    ast_nodes.bind(3);
    _ast_ssbos.ir_codegen->bind(0);



    if (job.dbg) {
        auto tokens_dmp = tokens->dump();
        print_tokens(tokens_dmp, tokens->size, *lang_tokens_parse_tree, *lang_tokens_parse_tree);
        free(tokens_dmp);
    }

    #ifdef BENCHMARKING
    auto bm_ = Benchmark("AST Exec");
    #endif


    auto types = Ssbo(666 * sizeof(GLuint)); // TODO: Get proper size.
    types.bind(6);

    _ast_ssbos.ast_parse_trees[0]->bind(4);

    shaders.literals.exec(ast_invocations_count);
    _ast_ssbos.ast_nodes->bind(1);

    for (int i = 0; i < 30; i++)
        shaders.ast.exec(ast_invocations_count);

    _ast_ssbos.ast_parse_trees[1]->bind(4);
    for (int i = 0; i < 200; i++)
        shaders.ast.exec(ast_invocations_count);

    #ifdef BENCHMARKING
    bm_.finalise();
    #endif

    if (job.dbg) {
        auto nodes = ast_nodes.dump();
        print_ast_nodes(nodes, ast_nodes.size - 1, *lang_tokens_parse_tree, yacc_parse_tree);
        free(nodes);

        // tokens->print_contents();

        // auto tokens_dmp = tokens->dump();
        // print_tokens(tokens_dmp, tokens->size, *lang_tokens_parse_tree, yacc_parse_tree);
        // free(tokens_dmp);

        // printf("AST NODES:\n");
        // ast_nodes.print_contents();


    }

    AstNode* ast_nodes_dmp = (AstNode*)ast_nodes.dump();

    // FIXME: there shouldn't nee to be a +5 here
    const GLuint output_buffer_size = fetch_entry_node_volume(tokens, ast_nodes_dmp) + 5;
    if (job.dbg) printf("Output buffer size: %d\n", output_buffer_size);


    Ssbo output_buffer = Ssbo(output_buffer_size * sizeof(GLuint));
    output_buffer.bind(1);

    codegen_shdr_exec(shaders, output_buffer_size);



    shaders.type_propagation.exec((types.size / 3) / 32);

    auto out_buf_dmp = (GLint*)output_buffer.dump();

    auto types_dmp = types.dump();
    print_types(types_dmp, types.size);

    auto _s = serialize_uir_to_readable((GLuint*) out_buf_dmp, output_buffer_size, *ir_tokens, source_str);
    if (job.dbg) printf("OUTPUT:\n%s\n", _s.c_str());


    // auto post_proc = postprocess((const GLint*)out_buf_dmp, output_buffer_size, var_reg, ir_parse_tree, source_str, ast_nodes_dmp);


    // auto s = serialize_uir_to_readable((GLuint*) post_proc.data, post_proc.size, *ir_tokens, source_str);

    // if (job.dbg) printf("OUTPUT:\n%s\n", s.c_str());

    delete_ast_ssbos(_ast_ssbos);
    free(out_buf_dmp);
    delete ir_tokens;
    delete lang_tokens_parse_tree;
    delete[] source.data;
    delete[] ast_nodes_dmp;
    delete tokens;

    #ifdef BENCHMARKING
    all.finalise();
    #endif

    #ifdef BENCHMARKING
    Benchmark::print_all();
    #endif

    // return s;
    return _s;
}