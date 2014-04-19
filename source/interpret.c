//
//  interpret.c
//  filagree
//

#include <unistd.h>
#include "vm.h"
#include "file.h"
#include "compile.h"
#include "interpret.h"

#define FG_MAX_INPUT   256
#define ERROR_USAGE    "usage: filagree [file]"


// run a file, using the same context
struct context *interpret_file_with(struct context *context, struct byte_array *path)
{
    if (NULL == context)
        context = context_new(NULL, true, true);
    
    struct byte_array *script = read_file(path, 0, 0);
    assert_message(NULL != script, "file not found: %s\n", byte_array_to_string(path));
    interpret_string(context, script);
    return context;
}

// run a script, using the same context
struct context *interpret_string_with(struct context *context, struct byte_array *script)
{
    if (NULL == context)
        context = context_new(NULL, true, true);
    
    interpret_string(context, script);
    return context;
}

bool run(struct context *context,
         struct byte_array *program,
         struct map *env,
         bool in_context);

void repl()
{
    char str[FG_MAX_INPUT];
    struct context *context = context_new(NULL, true, true);

    for (;;) {
        fflush(stdin);
        str[0] = 0;
        printf("f> ");
        if (!fgets(str, FG_MAX_INPUT, stdin)) {
            if (feof(stdin))
                return;
            if (ferror(stdin)) {
                printf("unknown error reading stdin\n");
                return;
            }
        }

        struct byte_array *input = byte_array_from_string(str);
        struct byte_array *program = build_string(input, NULL);
        if (!setjmp(trying))
            run(context, program, NULL, true);
        byte_array_del(input);
        byte_array_del(program);
    }

    context_del(context);
}

void interpret_string(struct context *context, struct byte_array *script)
{
    struct byte_array *program = build_string(script, NULL);
    execute_with(context, program, true);
    byte_array_del(program);
}

void interpret_file(struct byte_array *path,
                    struct byte_array *args)
{
    struct byte_array *program;

    if (NULL != args)
    {
        struct byte_array *source = read_file(path, 0, 0);
        if (NULL == source)            
            return;
        byte_array_append(args, source);
        program = build_string(args, path);
    }
    else
    {
        program = build_file(path);
    }

#ifdef DEBUG
    //display_program(program);
#endif

    execute(program);
    byte_array_del(program);
}

#ifdef FG_MAIN // define FG_MAIN if filagree is the main app and not just a library

#include <signal.h>

void sig_handler(const int sig)
{
	printf("\nSIGINT handled.\n");
    exit(1);
}

struct byte_array *arg2ba(int argc, char **argv)
{
    if (argc < 3)
        return NULL;
    return byte_array_from_string(argv[2]);
}

int main (int argc, char** argv)
{
	struct sigaction act, oact; // for handling ctrl-c
	act.sa_handler = sig_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, &oact);
    
    if (1 == argc)
        repl();
    else
    {
        struct byte_array *args = arg2ba(argc, argv);
        struct byte_array *path = byte_array_from_string(argv[1]);

        interpret_file(path, args);

        if (NULL != args)
            byte_array_del(args);
        byte_array_del(path);
    }
}

#endif // FG_MAIN

