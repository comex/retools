#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include "margp.h"

static char *buf, *ptr;
static size_t buf_size = 0;
static bool printed = false;
static pcre *re;
static char *pattern;
static char **files;
static char *delimiter = "--";
static char stdin_fn;
static int pcre_options = PCRE_MULTILINE;
static bool invert_match, ignore_case, no_multiline, dotall, double_newline, color;

struct margp_meta argp[] = {
    MARGP_OPT0('v', "invert-match",
               "Select lines not matching the pattern.",
               &invert_match),
    MARGP_OPT0('i', "ignore-case",
               "Perform case insensitive matching.",
               &ignore_case),
    MARGP_OPT0('M', "no-multiline",
               "^ and $ refer to the delimited block rather than a line within the block.",
               &no_multiline),
    MARGP_OPT0('s', "dotall",
               ". can include a newline",
               &dotall),
    MARGP_OPT0('n', "double-newline",
               "Blocks are delimited by an \\n\\n rather than \\n--\\n.",
               &double_newline),
    MARGP_OPT1('d', "delimiter",
               "Specify a delimiter (default --).",
               margp_str, &delimiter),
    MARGP_OPT0('c', "color",
               "Colored output.",
               &color),
    MARGP_ARG("pattern", margp_str, &pattern),
    MARGP_ARG("files", margp_str_vec, &files),
    MARGP_END
};

static void finish() {
    int range[2];
    int r = pcre_exec(re, NULL, buf, (int) (ptr - buf), 0, 0, range, 2);
    if(invert_match || !color) {
        if((r >= 0) == !invert_match) {
            if(printed) printf("%s", delimiter);
            printed = true;
            fwrite(buf, ptr - buf, 1, stdout);
        }
    } else if(r >= 0) {
        if(printed) printf("%s", delimiter);
        printed = true;
        int old = 0;
        do {
            fwrite(buf + old, range[0] - old, 1, stdout);
            printf("\x1b[7m");
            fwrite(buf + range[0], range[1] - range[0], 1, stdout);
            printf("\x1b[27m");
            old = range[1];
            r = pcre_exec(re, NULL, buf, (int) (ptr - buf), range[1], 0, range, 2);   
        } while(r >= 0);
        fwrite(buf + old, ptr - (buf + old), 1, stdout);
    }
    ptr = buf;
}

static void do_file(FILE *fp) {
    while(fgets(ptr, buf + buf_size - ptr, fp)) {
        if(!*ptr) break;
        if(!strcmp(ptr, delimiter)) {
            finish();
        } else {
            ptr += strlen(ptr);
            if(ptr + 1 == buf + buf_size) {
                ptrdiff_t offset = ptr - buf;
                buf_size *= 2;
                buf = realloc(buf, buf_size);
                ptr = buf + offset;
            }
        }
    }
    if(ferror(fp)) {
        fprintf(stderr, "Read error\n");
        exit(1);
    }
    finish();
}

int main(int argc, char **argv) {
    margp(argp, argv);

    if(ignore_case)
        pcre_options |= PCRE_CASELESS;
    if(dotall)
        pcre_options |= PCRE_DOTALL;
    if(no_multiline)
        pcre_options &= ~PCRE_MULTILINE;
    if(double_newline)
        delimiter = "";
    char *del = malloc(strlen(delimiter) + 2);
    sprintf(del, "%s\n", delimiter);
    delimiter = del;

    const char *err; int erroffset;
    re = pcre_compile(pattern, pcre_options, &err, &erroffset, NULL);

    if(!re) {
        fprintf(stderr, "error at %d: %s\n", erroffset, err);
        return 1;
    }

    buf_size = 65536;
    buf = ptr = malloc(buf_size);

    if(*files) {
        for(char **filep = files; *filep; filep++) {
            FILE *fp = fopen(*filep, "r");
            if(!fp) {
                perror("Couldn't open file");
                return 1;
            }
            do_file(fp);
        }
    } else {
        do_file(stdin);
    }

}
