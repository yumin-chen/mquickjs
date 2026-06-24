/*
 * Micro QuickJS compiler
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>

#include "cutils.h"
#include "mquickjs.h"

/* JS function declarations for the compiler to be able to use the stdlib */
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#include "mqjs_stdlib.h"

/* these are only for the compiler's own context */
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }

static uint8_t *load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    if (!buf) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    if (fread(buf, 1, buf_len, f) != buf_len) {
        fprintf(stderr, "Could not read %s\n", filename);
        exit(1);
    }
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, stderr);
}

static void dump_error(JSContext *ctx)
{
    JSValue obj;
    obj = JS_GetException(ctx);
    JS_PrintValueF(ctx, obj, JS_DUMP_LONG);
    fprintf(stderr, "\n");
}

typedef struct {
    uint8_t *data;
    uint32_t len;
    union {
        JSBytecodeHeader hdr;
#if JSW == 8
        JSBytecodeHeader32 hdr32;
#endif
    };
    int hdr_len;
} CompiledBytecode;

static void compile_file(CompiledBytecode *bc, const char *filename, const char *expr,
                         size_t mem_size, int parse_flags, BOOL force_32bit)
{
    uint8_t *mem_buf;
    JSContext *ctx;
    uint8_t *eval_str;
    int eval_len;
    JSValue val;
    const uint8_t *data_buf;
    uint32_t data_len;

    mem_buf = malloc(mem_size);
    if (!mem_buf) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    ctx = JS_NewContext2(mem_buf, mem_size, &js_stdlib, TRUE);
    JS_SetLogFunc(ctx, js_log_func);

    if (expr) {
        eval_str = (uint8_t *)strdup(expr);
        eval_len = strlen(expr);
        filename = "<cmdline>";
    } else {
        eval_str = load_file(filename, &eval_len);
    }

    val = JS_Parse(ctx, (char *)eval_str, eval_len, filename, parse_flags);
    free(eval_str);
    if (JS_IsException(val)) {
        dump_error(ctx);
        exit(1);
    }
    JSGCRef val_ref;
    JS_PUSH_VALUE(ctx, val);

#if JSW == 8
    if (force_32bit) {
        if (JS_PrepareBytecode64to32(ctx, &bc->hdr32, &data_buf, &data_len, val)) {
            fprintf(stderr, "Could not convert the bytecode from 64 to 32 bits\n");
            exit(1);
        }
        bc->hdr_len = sizeof(JSBytecodeHeader32);
    } else
#endif
    {
        JS_PrepareBytecode(ctx, &bc->hdr, &data_buf, &data_len, val);
        JS_RelocateBytecode2(ctx, &bc->hdr, (uint8_t *)data_buf, data_len, 0, FALSE);
        bc->hdr_len = sizeof(JSBytecodeHeader);
    }

    bc->data = malloc(data_len);
    if (!bc->data) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    memcpy(bc->data, data_buf, data_len);
    bc->len = data_len;

    JS_POP_VALUE(ctx, val);
    JS_FreeContext(ctx);
    free(mem_buf);
}

static void output_c(FILE *f, CompiledBytecode *bc, size_t mem_size)
{
    int i;
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <string.h>\n");
    fprintf(f, "#include <inttypes.h>\n");
    fprintf(f, "#include <sys/time.h>\n");
    fprintf(f, "#include \"mquickjs.h\"\n");
    fprintf(f, "\n");
    fprintf(f, "/* required for the stdlib */\n");
    fprintf(f, "JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    fprintf(f, "\n");
    fprintf(f, "#include \"mqjs_stdlib.h\"\n");
    fprintf(f, "\n");
    fprintf(f, "/* dummy or real implementation of the required functions */\n");
    fprintf(f, "JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {\n");
    fprintf(f, "    for(int i = 0; i < argc; i++) {\n");
    fprintf(f, "        if (i != 0) putchar(' ');\n");
    fprintf(f, "        if (JS_IsString(ctx, argv[i])) {\n");
    fprintf(f, "            JSCStringBuf buf;\n");
    fprintf(f, "            size_t len;\n");
    fprintf(f, "            const char *str = JS_ToCStringLen(ctx, &len, argv[i], &buf);\n");
    fprintf(f, "            fwrite(str, 1, len, stdout);\n");
    fprintf(f, "        } else {\n");
    fprintf(f, "            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "    putchar('\\n');\n");
    fprintf(f, "    return JS_UNDEFINED;\n");
    fprintf(f, "}\n");
    fprintf(f, "JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { JS_GC(ctx); return JS_UNDEFINED; }\n");
    fprintf(f, "JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {\n");
    fprintf(f, "    struct timeval tv; gettimeofday(&tv, NULL);\n");
    fprintf(f, "    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));\n");
    fprintf(f, "}\n");
    fprintf(f, "JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {\n");
    fprintf(f, "    struct timeval tv; gettimeofday(&tv, NULL);\n");
    fprintf(f, "    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));\n");
    fprintf(f, "}\n");
    fprintf(f, "JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_ThrowInternalError(ctx, \"load() not supported\"); }\n");
    fprintf(f, "JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_ThrowInternalError(ctx, \"setTimeout() not supported\"); }\n");
    fprintf(f, "JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }\n");
    fprintf(f, "\n");
    fprintf(f, "static const uint8_t qjs_bytecode[] = {\n");
    for (i = 0; i < bc->hdr_len; i++) {
        fprintf(f, " 0x%02x,", ((uint8_t *)&bc->hdr)[i]);
        if ((i & 15) == 15) fprintf(f, "\n");
    }
    for (i = 0; i < bc->len; i++) {
        fprintf(f, " 0x%02x,", bc->data[i]);
        if (((i + bc->hdr_len) & 15) == 15) fprintf(f, "\n");
    }
    fprintf(f, "\n};\n\n");

    fprintf(f, "static void js_log_func(void *opaque, const void *buf, size_t buf_len) {\n");
    fprintf(f, "    fwrite(buf, 1, buf_len, stdout);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "int main(int argc, char **argv) {\n");
    fprintf(f, "    size_t mem_size = %zu;\n", mem_size);
    fprintf(f, "    uint8_t *mem_buf = malloc(mem_size);\n");
    fprintf(f, "    JSContext *ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);\n");
    fprintf(f, "    JS_SetLogFunc(ctx, js_log_func);\n");
    fprintf(f, "    {\n");
    fprintf(f, "        struct timeval tv;\n");
    fprintf(f, "        gettimeofday(&tv, NULL);\n");
    fprintf(f, "        JS_SetRandomSeed(ctx, ((uint64_t)tv.tv_sec << 32) ^ tv.tv_usec);\n");
    fprintf(f, "    }\n");
    fprintf(f, "    uint8_t *bc_copy = malloc(sizeof(qjs_bytecode));\n");
    fprintf(f, "    memcpy(bc_copy, qjs_bytecode, sizeof(qjs_bytecode));\n");
    fprintf(f, "    if (JS_RelocateBytecode(ctx, bc_copy, sizeof(qjs_bytecode))) {\n");
    fprintf(f, "        fprintf(stderr, \"Could not relocate bytecode\\n\");\n");
    fprintf(f, "        return 1;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    JSValue val = JS_LoadBytecode(ctx, bc_copy);\n");
    fprintf(f, "    JSGCRef val_ref;\n");
    fprintf(f, "    JS_PUSH_VALUE(ctx, val);\n");
    fprintf(f, "    if (argc > 1) {\n");
    fprintf(f, "        JSValue arr = JS_NewArray(ctx, argc - 1);\n");
    fprintf(f, "        JSGCRef arr_ref;\n");
    fprintf(f, "        JS_PUSH_VALUE(ctx, arr);\n");
    fprintf(f, "        for (int i = 1; i < argc; i++) {\n");
    fprintf(f, "            JS_SetPropertyUint32(ctx, arr, i - 1, JS_NewString(ctx, argv[i]));\n");
    fprintf(f, "        }\n");
    fprintf(f, "        JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), \"scriptArgs\", arr);\n");
    fprintf(f, "        JS_POP_VALUE(ctx, arr);\n");
    fprintf(f, "    }\n");
    fprintf(f, "    val = JS_Run(ctx, val);\n");
    fprintf(f, "    JS_POP_VALUE(ctx, val);\n");
    fprintf(f, "    if (JS_IsException(val)) {\n");
    fprintf(f, "        JSValue obj = JS_GetException(ctx);\n");
    fprintf(f, "        JS_PrintValueF(ctx, obj, JS_DUMP_LONG);\n");
    fprintf(f, "        printf(\"\\n\");\n");
    fprintf(f, "        return 1;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    JS_FreeContext(ctx);\n");
    fprintf(f, "    free(mem_buf);\n");
    fprintf(f, "    free(bc_copy);\n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
}

static int execlp_res(const char *cmd, ...)
{
    va_list ap;
    char *args[128];
    int i, status;
    pid_t pid;

    va_start(ap, cmd);
    args[0] = (char *)cmd;
    for (i = 1; i < 127; i++) {
        args[i] = va_arg(ap, char *);
        if (!args[i])
            break;
    }
    args[i] = NULL;
    va_end(ap);

    pid = fork();
    if (pid == 0) {
        execvp(cmd, args);
        perror(cmd);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
    } else {
        perror("fork");
        return -1;
    }
}

static void help(void)
{
    printf("MicroQuickJS Compiler\n"
           "usage: mqjsc [options] [file]\n"
           "-h  --help            list options\n"
           "-e  --eval EXPR       evaluate EXPR\n"
           "-o FILE               output executable to FILE (default = a.out)\n"
           "-m32                  force 32 bit bytecode output\n"
           "    --memory-limit n  limit the memory usage of the compiled program to 'n' bytes\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int optind;
    const char *out_filename = "a.out";
    const char *expr = NULL;
    const char *infile = NULL;
    size_t mem_size = 16 << 20;
    BOOL force_32bit = FALSE;
    CompiledBytecode bc;
    int parse_flags = 0;

    optind = 1;
    while (optind < argc && *argv[optind] == '-') {
        const char *arg = argv[optind] + 1;
        const char *longopt = "";
        if (!*arg)
            break;
        optind++;
        if (*arg == '-') {
            longopt = arg + 1;
            arg += strlen(arg);
            if (!*longopt)
                break;
        }
        for (; *arg || *longopt; longopt = "") {
            char opt = *arg;
            if (opt)
                arg++;
            if (opt == 'h' || opt == '?' || !strcmp(longopt, "help")) {
                help();
            }
            if (opt == 'o') {
                if (*arg) {
                    out_filename = arg;
                    break;
                }
                if (optind < argc) {
                    out_filename = argv[optind++];
                    break;
                }
                fprintf(stderr, "missing filename for -o\n");
                exit(2);
            }
            if (opt == 'e' || !strcmp(longopt, "eval")) {
                if (*arg) {
                    expr = arg;
                    break;
                }
                if (optind < argc) {
                    expr = argv[optind++];
                    break;
                }
                fprintf(stderr, "missing expression for -e\n");
                exit(2);
            }
            if (opt == 'm' && !strcmp(arg, "32")) {
                force_32bit = TRUE;
                arg += strlen(arg);
                continue;
            }
            if (!strcmp(longopt, "memory-limit")) {
                char *p;
                double count;
                if (optind >= argc) {
                    fprintf(stderr, "expecting memory limit\n");
                    exit(1);
                }
                count = strtod(argv[optind++], &p);
                switch (tolower((unsigned char)*p)) {
                case 'g': count *= 1024;
                case 'm': count *= 1024;
                case 'k': count *= 1024;
                default:
                    mem_size = (size_t)(count);
                    break;
                }
                continue;
            }
            if (opt) {
                fprintf(stderr, "mqjsc: unknown option '-%c'\n", opt);
            } else {
                fprintf(stderr, "mqjsc: unknown option '--%s'\n", longopt);
            }
            help();
        }
    }

    if (optind < argc) {
        infile = argv[optind++];
    }

    if (!infile && !expr) {
        help();
    }

    compile_file(&bc, infile, expr, mem_size, parse_flags, force_32bit);

    char c_filename[1024];
    snprintf(c_filename, sizeof(c_filename), "%s.c", out_filename);
    FILE *f = fopen(c_filename, "w");
    if (!f) {
        perror(c_filename);
        exit(1);
    }
    output_c(f, &bc, mem_size);
    fclose(f);

    const char *cc = getenv("CC");
    if (!cc) cc = "gcc";

    if (execlp_res(cc, "-O2", "-o", out_filename, c_filename,
                  "mquickjs.o", "dtoa.o", "libm.o", "cutils.o",
                  "-lm", NULL) != 0) {
        fprintf(stderr, "C compilation failed\n");
        exit(1);
    }

    unlink(c_filename);
    free(bc.data);
    return 0;
}
