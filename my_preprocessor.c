/*
 * my_preprocessor.c
 *
 * A simple C preprocessor.
 *   Step 1: remove comments
 *   Step 2: file inclusion   (#include "file.h")
 *   Step 3: macro substitution (#define, #undef)
 *
 * Usage:  ./my_preprocessor file.c
 * Output: file.i
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE    4096    /* longest line we can handle        */
#define MAX_MACROS  500     /* how many #define macros we store  */
#define MAX_PARAMS  10      /* parameters of one macro           */
#define MAX_NAME    64      /* length of a macro / param name    */
#define MAX_BODY    1024    /* length of a macro body            */
#define MAX_ARG     256     /* length of one macro argument      */
#define MAX_DEPTH   20      /* nested #include / macro limit     */
#define MAX_IF      50      /* nested #if limit                  */

typedef struct {
    char name[MAX_NAME];
    int  is_function;                   /* 1 for  SQUARE(x)  type      */
    int  param_count;
    char params[MAX_PARAMS][MAX_NAME];
    char body[MAX_BODY];
    int  busy;                          /* 1 while being expanded      */
} Macro;

static Macro macros[MAX_MACROS];
static int   macro_count = 0;

static int cond_stack[MAX_IF];          /* 1 = keep lines, 0 = skip   */
static int cond_top = 0;

void expand_macros(const char *line, char *out, int depth);
int  process_file(const char *filename, FILE *out, int depth);

/* ================================================================== */
/* Step 1: remove comments                                            */
/* ================================================================== */

/*
 * Removes  // line comments  and  / * block comments * /.
 * Anything inside "strings" or 'c'har constants is not touched,
 * so  "http://abc"  stays as it is.
 */
char *remove_comments(const char *src)
{
    size_t len = strlen(src);
    size_t i = 0, j = 0;
    char *out = malloc(len + 1);

    if (out == NULL)
        return NULL;

    while (i < len) {
        if (src[i] == '"' || src[i] == '\'') {
            /* string or char constant: copy it unchanged */
            char quote = src[i];
            out[j++] = src[i++];
            while (i < len && src[i] != quote && src[i] != '\n') {
                if (src[i] == '\\' && i + 1 < len)
                    out[j++] = src[i++];        /* escape like \" */
                out[j++] = src[i++];
            }
            if (i < len && src[i] == quote)
                out[j++] = src[i++];
        } else if (src[i] == '/' && src[i + 1] == '/') {
            /* line comment: skip till end of line */
            while (i < len && src[i] != '\n')
                i++;
        } else if (src[i] == '/' && src[i + 1] == '*') {
            /* block comment: replace with one space,
               keep the newlines so line count stays same */
            out[j++] = ' ';
            i += 2;
            while (i < len && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n')
                    out[j++] = '\n';
                i++;
            }
            i += 2;
        } else {
            out[j++] = src[i++];
        }
    }
    out[j] = '\0';
    return out;
}

/* ================================================================== */
/* Step 3: macro substitution                                         */
/*   #define PI 3.14                 object-like macro                */
/*   #define SQUARE(x) ((x) * (x))   function-like macro              */
/*   #define STR(x) #x               # makes a string   ->  "x"       */
/*   #define CAT(a, b) a ## b        ## joins two words ->  ab        */
/* ================================================================== */

/* append n characters of s to out (out can hold MAX_LINE chars) */
static void append(char *out, size_t *j, const char *s, size_t n)
{
    while (n-- > 0 && *j < MAX_LINE - 1)
        out[(*j)++] = *s++;
    out[*j] = '\0';
}

/* copy a "string" or 'c' constant unchanged; returns pointer after it */
static const char *copy_literal(const char *p, char *out, size_t *j)
{
    char quote = *p;

    append(out, j, p++, 1);
    while (*p && *p != quote) {
        if (*p == '\\' && p[1])
            append(out, j, p++, 1);
        append(out, j, p++, 1);
    }
    if (*p)
        append(out, j, p++, 1);
    return p;
}

static void trim(char *s)
{
    char *start = s;
    size_t n;

    while (*start == ' ' || *start == '\t')
        start++;
    memmove(s, start, strlen(start) + 1);
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

/* read a C identifier (letters, digits, _) into name; moves *p after it */
int read_identifier(const char **p, char *name)
{
    const char *s = *p;
    int k = 0;

    if (!(isalpha((unsigned char)*s) || *s == '_')) {
        name[0] = '\0';
        return 0;
    }
    while (isalnum((unsigned char)*s) || *s == '_') {
        if (k < MAX_NAME - 1)
            name[k++] = *s;
        s++;
    }
    name[k] = '\0';
    *p = s;
    return k;
}

static int find_macro(const char *name)
{
    int i;
    for (i = 0; i < macro_count; i++)
        if (strcmp(macros[i].name, name) == 0)
            return i;
    return -1;
}

int is_macro_defined(const char *name)
{
    return find_macro(name) >= 0;
}

/* text is everything after "#define" */
void add_macro(const char *text)
{
    Macro m;
    const char *p = text;
    int index;

    memset(&m, 0, sizeof(m));
    while (*p == ' ' || *p == '\t')
        p++;
    if (read_identifier(&p, m.name) == 0) {
        printf("Error: #define needs a macro name\n");
        return;
    }

    /* '(' right after the name -> function-like macro */
    if (*p == '(') {
        m.is_function = 1;
        p++;
        while (*p && *p != ')') {
            char param[MAX_NAME];
            if (read_identifier(&p, param) > 0) {
                if (m.param_count < MAX_PARAMS)
                    strcpy(m.params[m.param_count++], param);
            } else {
                p++;                     /* skip ',' and spaces */
            }
        }
        if (*p == ')')
            p++;
    }

    strncpy(m.body, p, MAX_BODY - 1);
    trim(m.body);

    index = find_macro(m.name);
    if (index >= 0)
        macros[index] = m;               /* redefine */
    else if (macro_count < MAX_MACROS)
        macros[macro_count++] = m;
    else
        printf("Error: too many macros\n");
}

void remove_macro(const char *name)
{
    int index = find_macro(name);
    if (index >= 0)
        macros[index] = macros[--macro_count];
}

/*
 * Read the arguments of a macro call.  p points at '('.
 * SQUARE(a + 1, f(x, y))  ->  "a + 1" and "f(x, y)"
 * Returns pointer after ')', or NULL if ')' is missing.
 */
static const char *read_arguments(const char *p, char args[][MAX_ARG],
                                  int *argc)
{
    int level = 0, k = 0;
    char quote = 0;

    *argc = 0;
    p++;                                 /* skip '(' */
    while (*p) {
        char c = *p;

        if (quote) {                     /* inside "..." */
            if (c == '\\' && p[1]) {
                if (k < MAX_ARG - 1 && *argc < MAX_PARAMS)
                    args[*argc][k++] = c;
                p++;
                c = *p;
            } else if (c == quote) {
                quote = 0;
            }
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '(') {
            level++;
        } else if (c == ')' && level > 0) {
            level--;
        } else if (c == ')' || c == ',') {
            /* end of one argument */
            if (*argc < MAX_PARAMS) {
                args[*argc][k] = '\0';
                trim(args[*argc]);
                (*argc)++;
            }
            k = 0;
            p++;
            if (c == ')')
                return p;
            continue;
        }

        if (k < MAX_ARG - 1 && *argc < MAX_PARAMS)
            args[*argc][k++] = c;
        p++;
    }
    return NULL;
}

static int param_index(const Macro *m, const char *name)
{
    int i;
    for (i = 0; i < m->param_count; i++)
        if (strcmp(m->params[i], name) == 0)
            return i;
    return -1;
}

/* put the arguments in place of the parameters in the macro body */
static void substitute(const Macro *m, char args[][MAX_ARG], char *result,
                       int depth)
{
    const char *p = m->body;
    char name[MAX_NAME];
    char temp[MAX_LINE];
    size_t j = 0;
    int i;

    result[0] = '\0';
    while (*p) {
        if (*p == '"' || *p == '\'') {
            p = copy_literal(p, result, &j);
        } else if (p[0] == '#' && p[1] == '#') {
            /* a ## b : remove the spaces around ## */
            while (j > 0 && (result[j - 1] == ' ' || result[j - 1] == '\t'))
                j--;
            result[j] = '\0';
            p += 2;
            while (*p == ' ' || *p == '\t')
                p++;
        } else if (*p == '#') {
            /* #x : put the argument inside quotes */
            p++;
            while (*p == ' ')
                p++;
            read_identifier(&p, name);
            i = param_index(m, name);
            append(result, &j, "\"", 1);
            append(result, &j, i >= 0 ? args[i] : name,
                   strlen(i >= 0 ? args[i] : name));
            append(result, &j, "\"", 1);
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            read_identifier(&p, name);
            i = param_index(m, name);
            if (i >= 0) {
                /* argument is expanded first: SQUARE(SQUARE(2)) */
                expand_macros(args[i], temp, depth + 1);
                append(result, &j, temp, strlen(temp));
            } else {
                append(result, &j, name, strlen(name));
            }
        } else {
            append(result, &j, p++, 1);
        }
    }
}

/*
 * Replace every macro in line. Result goes to out (size MAX_LINE).
 * The result of a macro is checked again for more macros
 * (e.g. AREA(r) uses PI and SQUARE).
 */
void expand_macros(const char *line, char *out, int depth)
{
    const char *p = line;
    size_t j = 0;

    out[0] = '\0';
    while (*p) {
        if (*p == '"' || *p == '\'') {
            p = copy_literal(p, out, &j);
        } else if (isdigit((unsigned char)*p)) {
            /* number like 10UL or 0x1F: copy whole */
            while (isalnum((unsigned char)*p) || *p == '_' || *p == '.')
                append(out, &j, p++, 1);
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            char name[MAX_NAME];
            char result[MAX_LINE];
            char temp[MAX_LINE];
            Macro *m;
            int index;

            read_identifier(&p, name);
            index = find_macro(name);
            if (index < 0 || macros[index].busy || depth >= MAX_DEPTH) {
                append(out, &j, start, (size_t)(p - start));
                continue;
            }
            m = &macros[index];

            if (!m->is_function) {
                strcpy(result, m->body);
            } else {
                char args[MAX_PARAMS][MAX_ARG];
                const char *q = p;
                const char *after;
                int argc;

                while (*q == ' ' || *q == '\t')
                    q++;
                if (*q != '(') {         /* name used without (...) */
                    append(out, &j, start, (size_t)(p - start));
                    continue;
                }
                after = read_arguments(q, args, &argc);
                if (after == NULL) {
                    printf("Error: missing ')' in call of %s\n", m->name);
                    append(out, &j, start, strlen(start));
                    break;
                }
                if (m->param_count == 0 && argc == 1 && args[0][0] == '\0')
                    argc = 0;            /* F() with no parameters */
                if (argc != m->param_count) {
                    printf("Error: macro %s needs %d argument(s), %d given\n",
                           m->name, m->param_count, argc);
                    append(out, &j, start, (size_t)(after - start));
                    p = after;
                    continue;
                }
                substitute(m, args, result, depth);
                p = after;
            }

            /* check the result again; m is busy so  #define X X+1  stops */
            m->busy = 1;
            expand_macros(result, temp, depth + 1);
            m->busy = 0;
            append(out, &j, temp, strlen(temp));
        } else {
            append(out, &j, p++, 1);
        }
    }
}

/* ================================================================== */
/* Step 2: file inclusion, and the main loop over lines               */
/*   #include "file.h"  -> the header is processed and copied here    */
/*   #include <stdio.h> -> system header, line is kept as it is       */
/*   #define / #undef   -> stored in / removed from the macro table   */
/*   #ifdef / #ifndef / #if / #else / #endif -> keep or skip lines    */
/*   normal line        -> macros replaced, written to the .i file    */
/* ================================================================== */

/* lines are kept only if every open #if level is true */
static int is_active(void)
{
    int i;
    for (i = 0; i < cond_top; i++)
        if (cond_stack[i] == 0)
            return 0;
    return 1;
}

static void push_condition(int value)
{
    if (cond_top < MAX_IF)
        cond_stack[cond_top++] = value;
    else
        printf("Error: too many nested #if\n");
}

/* read the whole file into memory ("\r" of Windows files removed) */
char *read_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    char *buf;
    long size;
    size_t n, i, j;

    if (fp == NULL)
        return NULL;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);

    for (i = 0, j = 0; i < n; i++)
        if (buf[i] != '\r')
            buf[j++] = buf[i];
    buf[j] = '\0';
    return buf;
}

/* #include "name"  ->  look for name next to the current file */
static void do_include(const char *p, const char *filename, FILE *out,
                       int depth)
{
    char name[256], path[512];
    const char *slash1, *slash2, *slash;
    int k = 0, dir_len = 0;

    if (*p == '<') {                     /* system header: keep line */
        fprintf(out, "#include %s\n", p);
        return;
    }
    if (*p != '"') {
        printf("%s: Error: #include expects \"file\" or <file>\n", filename);
        return;
    }

    p++;
    while (*p && *p != '"' && k < (int)sizeof(name) - 1)
        name[k++] = *p++;
    name[k] = '\0';

    /* folder of the current file, e.g. "src/" from "src/a.c" */
    slash1 = strrchr(filename, '/');
    slash2 = strrchr(filename, '\\');
    slash = slash1;
    if (slash2 != NULL && (slash == NULL || slash2 > slash))
        slash = slash2;
    if (slash != NULL)
        dir_len = (int)(slash - filename) + 1;

    snprintf(path, sizeof(path), "%.*s%s", dir_len, filename, name);
    process_file(path, out, depth + 1);
}

static void handle_directive(const char *p, const char *filename, FILE *out,
                             int depth)
{
    char word[32], name[MAX_NAME];
    int k = 0;

    /* directive word: include, define, ifdef ... */
    while (*p == ' ' || *p == '\t')
        p++;
    while (isalpha((unsigned char)*p) && k < 31)
        word[k++] = *p++;
    word[k] = '\0';
    while (*p == ' ' || *p == '\t')
        p++;
    if (word[0] == '\0')
        return;                          /* a line with only "#" */

    /* conditions are checked even inside skipped parts */
    if (strcmp(word, "ifdef") == 0 || strcmp(word, "ifndef") == 0) {
        int defined;
        read_identifier(&p, name);
        defined = is_macro_defined(name);
        push_condition(strcmp(word, "ifdef") == 0 ? defined : !defined);
        return;
    }
    if (strcmp(word, "if") == 0) {
        char value[MAX_LINE];
        expand_macros(p, value, 0);      /* #if DEBUG  ->  #if 1 */
        push_condition(atoi(value) != 0);
        return;
    }
    if (strcmp(word, "else") == 0) {
        if (cond_top > 0)
            cond_stack[cond_top - 1] = !cond_stack[cond_top - 1];
        else
            printf("%s: Error: #else without #if\n", filename);
        return;
    }
    if (strcmp(word, "endif") == 0) {
        if (cond_top > 0)
            cond_top--;
        else
            printf("%s: Error: #endif without #if\n", filename);
        return;
    }

    if (!is_active())
        return;

    if (strcmp(word, "include") == 0) {
        do_include(p, filename, out, depth);
    } else if (strcmp(word, "define") == 0) {
        add_macro(p);
    } else if (strcmp(word, "undef") == 0) {
        read_identifier(&p, name);
        remove_macro(name);
    } else {
        fprintf(out, "#%s %s\n", word, p);   /* e.g. #pragma: keep it */
    }
}

int process_file(const char *filename, FILE *out, int depth)
{
    char *text, *code, *line;
    char logical[MAX_LINE];
    char expanded[MAX_LINE];

    if (depth > MAX_DEPTH) {
        printf("Error: #include nested too deeply (%s)\n", filename);
        return -1;
    }

    text = read_file(filename);
    if (text == NULL) {
        printf("Error: cannot open file %s\n", filename);
        return -1;
    }

    /* Step 1: remove comments */
    code = remove_comments(text);
    free(text);
    if (code == NULL)
        return -1;

    line = code;
    while (*line) {
        char *p;

        /* join lines ending with '\' into one logical line */
        logical[0] = '\0';
        for (;;) {
            char *nl = strchr(line, '\n');
            size_t n;

            if (nl != NULL)
                *nl = '\0';
            strncat(logical, line, sizeof(logical) - strlen(logical) - 1);
            line = (nl != NULL) ? nl + 1 : line + strlen(line);

            n = strlen(logical);
            if (n > 0 && logical[n - 1] == '\\' && nl != NULL) {
                logical[n - 1] = ' ';
                continue;
            }
            break;
        }

        p = logical;
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '#') {
            handle_directive(p + 1, filename, out, depth);
        } else if (is_active()) {
            /* Step 3: replace macros */
            expand_macros(logical, expanded, 0);
            fprintf(out, "%s\n", expanded);
        }
    }

    free(code);
    return 0;
}

/* ================================================================== */
/* main                                                               */
/* ================================================================== */
int main(int argc, char *argv[])
{
    char out_name[512];
    size_t len;
    FILE *out;

    if (argc != 2) {
        printf("Usage: %s <file.c>\n", argv[0]);
        return 1;
    }

    /* input must be a .c file */
    len = strlen(argv[1]);
    if (len < 3 || strcmp(argv[1] + len - 2, ".c") != 0) {
        printf("Error: input must be a .c file\n");
        return 1;
    }
    if (len >= sizeof(out_name)) {
        printf("Error: file name too long\n");
        return 1;
    }

    /* file.c  ->  file.i */
    strcpy(out_name, argv[1]);
    out_name[len - 1] = 'i';

    out = fopen(out_name, "w");
    if (out == NULL) {
        printf("Error: cannot create %s\n", out_name);
        return 1;
    }

    if (process_file(argv[1], out, 0) != 0) {
        fclose(out);
        remove(out_name);
        return 1;
    }

    fclose(out);
    printf("Done: %s -> %s\n", argv[1], out_name);
    return 0;
}
