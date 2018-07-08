#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mpc.h"

/* If we're compiling on Windows, compile these functions
 * Note: Windows defaults to readline / history behavior;
 * Linux and MacOS need to use these libraries
 */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#ifdef __linux__
#include <editline/history.h>
#endif
#endif

long eval_op(long x, char *op, long y) {
    if (strcmp(op, "+") == 0 || strcmp(op, "add") == 0) { return x + y; }
    if (strcmp(op, "-") == 0 || strcmp(op, "sub") == 0) { return x - y; }
    if (strcmp(op, "*") == 0 || strcmp(op, "mul") == 0) { return x * y; }
    if (strcmp(op, "/") == 0 || strcmp(op, "div") == 0) { return x / y; }
    if (strcmp(op, "%") == 0 || strcmp(op, "mod") == 0) { return x % y; }
    if (strcmp(op, "^") == 0 || strcmp(op, "exp") == 0) { return powl(x,y); }
    return 0;
}

long eval(mpc_ast_t *t) {

    /* If tagged as a number return it directly. */
    if(strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    /* The operator is always second child. */
    char *op = t->children[1]->contents;

    /* We store the third child in `x` */
    long x = eval(t->children[2]);

    /* Iterate the remaining children and combine. */
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main (int argc, char *argv[]) {
    /* Create Some Parsers */
    mpc_parser_t *Number    = mpc_new("number");
    mpc_parser_t *Operator  = mpc_new("operator");
    mpc_parser_t *Expr      = mpc_new("expr");
    mpc_parser_t *Lispy     = mpc_new("lispy");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                           \
            number      : /-?[0-9]+(\\.[0-9]+)?/ ;                  \
            operator    : '+' | '-' | '*' | '/'                     \
                        | '%' | '^'                                 \
                        | \"add\" | \"sub\" | \"mul\" | \"div\"     \
                        | \"mod\" | \"exp\" ;                       \
            expr        : <number> | '(' <operator> <expr>+ ')' ;   \
            lispy       : /^/ <operator> <expr>+ /$/ ;              \
        ",
        Number, Operator, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    /* In a never ending loop */
    while (1) {
        /* Output prompt and get input */
        char *input = readline("lispy> ");

        /* Add input to history */
        add_history(input);

        /* Attempt to parse user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On success, print the AST */
            //mpc_ast_print(r.output);
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise, print the error */
            mpc_err_print(r.output);
            mpc_err_delete(r.output);
        }

        /* Free retrieved input */
        free (input);
    }

    /* Undefine and Delete our Parsers */
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
