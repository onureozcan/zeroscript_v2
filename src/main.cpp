#include <iostream>
#include <antlr4-runtime.h>
#include <fstream>
#include <stack>
#include <cstring>

using namespace std;
using namespace antlr4;
using namespace tree;

char *compile_file(const char *filename, size_t *len);

#include "common/common.h"
#include "compiler/Compiler.cpp"
#include "interpreter/interpreter_dd.c"

char *compile_file(const char *filename, size_t *len) {
    std::ifstream stream;
    stream.open(filename);
    ANTLRInputStream input(stream);
    zeroscriptLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    zeroscriptParser parser(&tokens);
    AstGenerator astGenerator = AstGenerator(parser.classDeclaration());
    ClassDeclaration *cls = astGenerator.getRootClass();
    Compiler compiler = Compiler(cls);
    size_t mlen = 0;
    char *bytes = compiler.toBytes(&mlen);
    *len = mlen;
    return bytes;
}

int main(int argc, const char *argv[]) {

    bool runMode = false;
    const char *compile_flag = NULL;
    const char *filename = NULL;
    const char *oFilename = NULL;
    char *class_path = NULL;
    //compile to file
    if (argc == 4) {
        compile_flag = argv[1];
        filename = argv[2];
        oFilename = argv[3];
    } else {
        //run as a script
        filename = argv[1];
        if (argc == 3)
            class_path = const_cast<char *>(argv[2]);
        runMode = true;
    }
    size_t len = 0;
    char *file_path = NULL;
    if (class_path == NULL) {
        file_path = (char *) (filename);
    } else {
        file_path = (char *) z_alloc_or_die(strlen(class_path) + strlen(filename) + 2);
        sprintf(file_path, "%s/%s", class_path, filename);
    }
    char *bytes = compile_file(file_path, &len);
    object_manager_init(class_path);
    if (runMode) {
        clock_t begin = clock();
        z_interpreter_state_t *initial_state = (z_interpreter_state_t *) z_alloc_or_die(sizeof(z_interpreter_state_t));
        initial_state->fsize = len;
        initial_state->byte_stream = bytes;
        initial_state->current_context = NULL;
        initial_state->instruction_pointer = NULL;
        //TODO:ensure that filename matches classname.
        char *class_name = (char *) (z_alloc_or_die(strlen(filename) + 1));
        strcpy(class_name, filename);
        class_name[strlen(filename) - 3] = 0;
        object_manager_register_object_type(class_name, bytes, len);
        initial_state->class_name = class_name;
        interpreter_run_static_constructor(bytes, class_name);
        initial_state = z_interpreter_run(initial_state);
        clock_t end = clock();
        double time_spent = (double) (end - begin) / CLOCKS_PER_SEC;
        printf("time spent: %lf\n", time_spent);
    } else {
        FILE *ofile = fopen(oFilename, "wb");
        fwrite(bytes, static_cast<size_t>(len), 1, ofile);
        fclose(ofile);
    }
    return 0;
}