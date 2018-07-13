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

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create Enumeration of Possible lval Types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM,
       LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR  };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare New lval (read: lisp value) Struct */
struct lval {
    int type;
    long num;
    /* Error and Symbol types have some string data */
    char *err;
    char *sym;
    lbuiltin fun;
    /* Count and Pointer to a list of "lval*" */
    int count;
    struct lval **cell;
};

/* Create a pointer to new number type lval */
lval *lval_num(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Create a pointer to new error type lval */
lval *lval_err(char *fmt, ...) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, fmt, va);

    /* Reallocate to number of bytes actually used */
    v->err = realloc(v->err, strlen(v->err)+1);

    /* cleanup our va list */
    va_end(va);

    return v;
}

/* Construct a pointer to a new Symbol lval */
lval *lval_sym(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval *lval_fun(lbuiltin func) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

/* A pointer to a new empty Sexpr lval */
lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* A pointer to a new empty Qexpr lval */
lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval *v) {
    switch (v->type) {
        /* Do nothing special for number or function type */
        case LVAL_NUM: break;
        case LVAL_FUN: break;

        /* For Err or Sym free the string data */
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        /* If Qexpr or Sexpr then delete all elements inside */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            /* Also free the memory allocated to contain the pointers */
            free(v->cell);
        break;
    }

    /* Free the memory allocated for the "lval" struct itself */
    free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_num(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval *lval_read(mpc_ast_t *t) {

    /* If Symbol or Number, return conversion to that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root (>) or sexpr then create empty list */
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval *lval_pop(lval *v, int i) {
    /* Find the item at "i" */
    lval *x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(lval*) * (v->count-i-1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval *lval_take(lval *v, int i) {
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        /* Print value contained within */
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Print an "lval" */
void lval_print(lval *v) {
    /* In the case the type is a number, print it
     * Then 'break' out of the switch. */
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_FUN: printf("<function>"); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval *v) { lval_print(v); putchar('\n'); }

lval *lval_copy(lval *v) {

    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {

        /* Copy Functions and Numbers directly */
        case LVAL_FUN: x->fun = v->fun; break;
        case LVAL_NUM: x->num = v->num; break;

        /* Copy Strings using malloc and strcpy */
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;

        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;

        /* Copy Lists by copying each sub-expression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval *) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;
    }

    return x;
}

struct lenv {
    int count;
    char **syms;
    lval **vals;
};

lenv *lenv_new(void) {
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval *lenv_get(lenv *e, lval *k) {

    /* Iterate over all items in environment */
    for (int i = 0; i < e->count; i++) {
        /* Check if the stored string matches the symbol string
         * If it does, return a copy of the value */
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    /* If no symbol found, return error */
    return lval_err("Unbound symbol '%s'", k->sym);
}

void lenv_put(lenv *e, lval *k, lval *v) {

    /* Iterate over all items in environment
     * To see if variable already exists */
    for (int i = 0; i < e->count; i++) {

        /* If variable is found delete item at that position
         * And replace with variable supplied by user */
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry found, allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval *err = lval_err(fmt, #__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_NUM_ARGS(func, args, num)       \
    LASSERT(args, args->count == num, \
        "Function '%s' passed too many arguments! " \
        "Got %i, Expected %i.", \
            func, args->count, 1);

#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, args->cell[index]->type == expect, \
            "Function '%s' passed incorrect type! " \
            "Got %s, Expected %s.", \
            func, args->cell[index]->type, expect);

#define LASSERT_NON_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
            "Function '%s' passed {} for argument %i." \
            func, index);

lval *builtin_op(lenv *e, lval *a, char *op);

lval *builtin_add(lenv *e, lval *a) {
    return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a) {
    return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a) {
    return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a) {
    return builtin_op(e, a, "/");
}

lval *builtin_op(lenv *e, lval *a, char *op) {

    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("op", a, i, LVAL_NUM);
    }

    /* Pop the first element */
    lval *x = lval_pop(a, 0);

    /* If no arguments and sub, then perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        lval *y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num;     }
        if (strcmp(op, "-") == 0) { x->num -= y->num;     }
        if (strcmp(op, "*") == 0) { x->num *= y->num;     }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by Zero!"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(a); return x;
}

lval *builtin_head(lenv *e, lval *a) {
    /* Check Error conditions */
    LASSERT_NUM_ARGS("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NON_EMPTY("head", a, 0)

    /* Otherwise take first argument */
    lval *v = lval_take(a,0);

    /* Delete all elements that are not head and return */
    while (v->count > 1) { lval_del(lval_pop(v,1)); }
    return v;
}

lval *builtin_tail(lenv *e, lval *a){
    /* Check Error conditions */
    LASSERT_NUM_ARGS("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NON_EMPTY("tail", a, 0);

    /* Take first argument */
    lval *v = lval_take(a, 0);

    /* Delete first element and return */
    lval_del(lval_pop(v,0));
    return v;
}

lval *builtin_list(lenv *e, lval *a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval *lval_join(lval *x, lval *y);

lval *builtin_join(lenv *e, lval *a) {

    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval *x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval *lval_join(lval *x, lval *y) {

    /* For each cell in 'y', add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

lval *lval_eval(lenv *e, lval *v);

lval *builtin_eval(lenv *e, lval *a) {
    LASSERT_NUM_ARGS("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv *e) {
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "add", builtin_add);
    lenv_add_builtin(e, "sub", builtin_sub);
    lenv_add_builtin(e, "mul", builtin_mul);
    lenv_add_builtin(e, "div", builtin_div);
}

lval *lval_eval_sexpr(lenv *e, lval *v){

    /* Evaluate Children */
    for (int i = 0; i< v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    /* Empty Expression */
    if (v->count == 0) { return v; }

    /* Single Expression */
    if (v->count == 1) { return lval_take(v, 0); }

    /* Ensure First Element is a function after evaluation */
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression does not start with symbol!");
    }

    /* Call builtin with operator */
    lval *result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv *e, lval *v) {

    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e,v);
        lval_del(v);
        return x;
    }
    /* Evaluate S-Expressions */
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    /* All other lval types remain the same */
    return v;
}

int main (int argc, char *argv[]) {
    /* Create Some Parsers */
    mpc_parser_t *Number    = mpc_new("number");
    mpc_parser_t *Symbol    = mpc_new("symbol");
    mpc_parser_t *Sexpr     = mpc_new("sexpr");
    mpc_parser_t *Qexpr     = mpc_new("qexpr");
    mpc_parser_t *Expr      = mpc_new("expr");
    mpc_parser_t *Lispy     = mpc_new("lispy");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                           \
            number      : /-?[0-9]+(\\.[0-9]+)?/ ;                  \
            symbol      : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
            sexpr       : '(' <expr>* ')' ;                         \
            qexpr       : '{' <expr>* '}' ;                         \
            expr        : <number> | <symbol> | <sexpr> | <qexpr> ; \
            lispy       : /^/ <expr>* /$/ ;                         \
        ",
              Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    lenv *e = lenv_new();
    lenv_add_builtins(e);

    /* In a never ending loop */
    while (1) {
        /* Output prompt and get input */
        char *input = readline("lispy> ");

        /* Add input to history */
        add_history(input);

        /* Attempt to parse user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval *result = lval_eval(e, lval_read(r.output));
            lval_println(result);
            lval_del(result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise, print the error */
            mpc_err_print(r.output);
            mpc_err_delete(r.output);
        }

        /* Free retrieved input */
        free (input);
    }
    lenv_del(e);

    /* Undefine and Delete our Parsers */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}
