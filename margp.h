#pragma once
// from http://github.com/comex/cbit
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MARGP_LINE_LEN 72

enum margp_meta_kind {
    margp_meta_opt,
    margp_meta_end,
    margp_meta_optarg,
    margp_meta_arg,
};

enum margp_meta_type {
    margp_str,
    margp_int,
    margp_str_vec
};

struct margp_meta_opt {
    char chr;
    const char *name;
    const char *desc;
    bool *outp;
};

struct margp_meta_arg {
    enum margp_meta_type type;
    const char *name;
    union {
        char **sp;
        int *ip;
        char ***svp;
        void *vp;
    } outp;
};

struct margp_meta {
    enum margp_meta_kind kind;
    union {
        struct margp_meta_opt opt;
        struct margp_meta_arg arg;
    } u;
};

#define MARGP_OUTP_SPEC_margp_str(outp) {.sp = outp}
#define MARGP_OUTP_SPEC_margp_int(outp) {.ip = outp}
#define MARGP_OUTP_SPEC_margp_str_vec(outp) {.svp = outp}

#define MARGP_OPT0(chr, name, desc, outp) \
    {margp_meta_opt, {.opt = {chr, name, desc, outp}}}
#define MARGP_OPT1(chr, name, desc, type, outp) \
    {margp_meta_opt, {.opt = {chr, name, desc, 0}}}, \
    {margp_meta_optarg, {.arg = {type, "val", \
                                 MARGP_OUTP_SPEC_##type(outp)}}}
#define MARGP_OPTN(chr, name, desc) \
    {margp_meta_opt, {.opt = {chr, name, desc, 0}}}

#define MARGP_OPTARG(name, type, outp) \
    {margp_meta_optarg, {.arg = {type, name, \
                                 MARGP_OUTP_SPEC_##type(outp)}}}
#define MARGP_ARG(name, type, outp) \
    {margp_meta_arg, {.arg = {type, name, \
                              MARGP_OUTP_SPEC_##type(outp)}}}
#define MARGP_END \
    {margp_meta_end, {.opt = {0, "help", "This help message", 0}}}

static void margp_usage(struct margp_meta *meta, char **argv) {
    struct margp_meta *m;
    int maxlen;

    printf("Usage: %s [OPTION]...", argv[0]);
    for(m = meta; m->kind != margp_meta_end; m++) {
        if(m->kind == margp_meta_arg) {
            struct margp_meta_arg *a = &m->u.arg;
            printf(" [%s]", a->name);
            if(a->type == margp_str_vec) {
                printf("...");
            }
        }
    }
    printf("\n");

    maxlen = 0;
    for(m = meta; ; m++) {
        struct margp_meta_opt *o;
        char *left, *lp;
        int len;
        if(m->kind > margp_meta_end)
            continue;
        o = &m->u.opt;
        lp = left = (char *) malloc(128);
        if(o->chr)
            lp += sprintf(lp, "-%c", o->chr);
        if(o->name) {
            if(o->chr)
                lp += sprintf(lp, ", ");
            lp += sprintf(lp, "--%s", o->name);
        }
        if(m->kind != margp_meta_end) {
            while(m[1].kind == margp_meta_optarg) {
                lp += sprintf(lp, " [%s]", (++m)->u.arg.name);
            }
        }
        o->name = left;
        len = lp - left;
        if(len > maxlen) maxlen = len;
        if(m->kind == margp_meta_end)
            break;
    }
    maxlen += 4;

    for(m = meta; ; m++) {
        struct margp_meta_opt *o;
        const char *dp;
        char c;
        if(m->kind > margp_meta_end)
            continue;
        o = &m->u.opt;
        printf("  %-*s", maxlen, o->name);
        if(o->desc) {
            int linelen = 2 + maxlen;
            for(dp = o->desc; (c = *dp); dp++) {
                if(c == ' ') {
                    char *next_space = strchr(dp + 1, ' ');
                    if(linelen > 2 + maxlen &&
                       next_space && linelen + (next_space - dp) > MARGP_LINE_LEN)
                        c = '\n';
                }
                putchar(c);
                linelen++;
                if(c == '\n') {
                    printf("  %-*s", maxlen, "");
                    linelen = 2 + maxlen;
                }
            }
        }
        printf("\n");
        if(m->kind == margp_meta_end)
            break;
    }

}

struct margp_str_vec {
    char **args;
    size_t len;
    size_t capacity;
};

static void margp_str_vec_append(struct margp_str_vec *str_vec, char *arg) {
    if(++str_vec->len > str_vec->capacity) {
        size_t new_capacity = 2 * str_vec->capacity + 4;
        str_vec->args = (char **) realloc(str_vec->args, new_capacity * sizeof(char *));
    }
    str_vec->args[str_vec->len - 1] = arg;
}

static struct margp_meta *margp_parse_arg(char *arg, struct margp_meta *m, struct margp_str_vec *str_vec) {
    struct margp_meta_arg *ma = &m->u.arg;
    long l;
    char *endptr;
    switch(ma->type) {
    case margp_str:
        *ma->outp.sp = arg;
        break; 
    case margp_int:
        l = strtol(arg, &endptr, 0);
        if(*endptr) {
            fprintf(stderr, "Error: bad integer %s\n", arg);
            exit(1);
        }
        if(l < INT_MIN || l > INT_MAX) {
            fprintf(stderr, "Error: integer out of range: %s\n", arg);
            exit(1);
        }
        *ma->outp.ip = l;
        break;
    case margp_str_vec:
        margp_str_vec_append(str_vec, arg);
        return m;
    default:
        abort();
    }
    return m + 1;
}

static bool margp_parse_opt(char ***argvp, char **pendingp, struct margp_meta *m) {
    struct margp_meta_opt *o = &m->u.opt;
    if(o->outp)
        *o->outp = true;
    while((++m)->kind == margp_meta_optarg) {
        char *arg = *pendingp ? *pendingp : *(*argvp)++;
        if(!arg) {
            fprintf(stderr, "Error: Not enough arguments\n");
            return true;
        }
        *pendingp = 0;
        margp_parse_arg(arg, m, 0);
    }
    return false;
}

static bool margp_run(struct margp_meta *meta, char **argv) {
    char *arg, *pending = 0;
    struct margp_meta *arg_m = meta;
    struct margp_str_vec str_vec = {0, 0, 0};
    char ***str_vec_outp = 0;
    bool dashdash = false;
    argv++;
    while((arg = pending ? pending : *argv++)) {
        pending = 0;
        if(arg[0] != '-' || dashdash) {
            /* an argument */
            if(arg_m->kind != margp_meta_arg) {
                do {
                    if(arg_m->kind == margp_meta_end) {
                        fprintf(stderr, "Error: Too many arguments (saw <%s>)\n", arg);
                        exit(1);
                    }
                    arg_m++;
                } while(arg_m->kind != margp_meta_arg);
            }
            arg_m = margp_parse_arg(arg, arg_m, &str_vec);
        } else {
            /* an option */
            if(arg[1] == '-') {
                if(arg[2] == '\0') {
                    dashdash = true;
                } else {
                    /* one long option */
                    struct margp_meta *m;
                    char *eq;
                    if((eq = strstr(arg, "="))) {
                        pending = eq + 1;
                        *eq = 0;
                    }
                    if(!strcmp(arg + 2, "help"))
                        return true;
                    for(m = meta; ; m++) {
                        struct margp_meta_opt *o;
                        if(m->kind > margp_meta_end)
                            continue;
                        o = &m->u.opt;
                        if(!strcmp(o->name, arg + 2)) {
                            if(margp_parse_opt(&argv, &pending, m))
                                return true;
                            break;
                        }
                        if(m->kind == margp_meta_end) {
                            fprintf(stderr, "Error: Unknown option: %s\n", arg);
                            exit(1);
                        }
                    }
                }
            } else {
                /* one or more short options
                   note that we support weird syntax because why not:
                     -ab a_arg b_arg */
                const char *ap;
                char c;
                for(ap = arg + 1; (c = *ap); ap++) {
                    struct margp_meta *m;
                    for(m = meta; ; m++) {
                        struct margp_meta_opt *o;
                        if(m->kind > margp_meta_end)
                            continue;
                        o = &m->u.opt;
                        if(o->chr == c) {
                            if(margp_parse_opt(&argv, &pending, m))
                                return true;
                            break;
                        }
                        if(m->kind == margp_meta_end) {
                            fprintf(stderr, "Error: Unknown option: -%c\n", c);
                            exit(1);
                        }
                    }
                }
            }
        }
    }
    while(arg_m->kind != margp_meta_end) {
        if(arg_m->kind == margp_meta_arg &&
           arg_m->u.arg.type == margp_str_vec) {
            str_vec_outp = arg_m->u.arg.outp.svp;
        }
        if((++arg_m)->kind == margp_meta_arg) {
            fprintf(stderr, "Error: Not enough arguments\n");
            return true;
        }
    }
    if(str_vec_outp) {
        margp_str_vec_append(&str_vec, 0);
        *str_vec_outp = str_vec.args;
    }
    return false;
}

static void margp(struct margp_meta *meta, char **argv) {
    bool need_help = margp_run(meta, argv);
    if(need_help) {
        margp_usage(meta, argv);
        exit(1);
    }
}

