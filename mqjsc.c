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
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "cutils.h"
#include "mquickjs.h"
#include "mqjs.h"
#include "mqjs_stdlib.h"

static void help(void)
{
    printf("MicroQuickJS Compiler\n"
           "usage: mqjsc [options] [file]\n"
           "-h  --help            list options\n"
           "-o FILE               set the output executable (default = a.out)\n"
           "-c                    only output the C source file\n"
           "-m32                  force 32 bit bytecode output\n"
           "    --memory-limit n  limit the memory usage of the generated executable to 'n' bytes\n"
           "--no-column           no column number in debug information\n");
    exit(1);
}

static void output_c(FILE *f, const char *c_name, const uint8_t *buf, int len)
{
    int i;
    fprintf(f, "const uint8_t %s[%d] = {", c_name, len);
    for(i = 0; i < len; i++) {
        if (i % 16 == 0)
            fprintf(f, "\n    ");
        fprintf(f, "0x%02x,", buf[i]);
    }
    fprintf(f, "\n};\n\n");
    fprintf(f, "const uint32_t %s_len = %d;\n\n", c_name, len);
}

static const char main_c_template[] =
    "#include <stdlib.h>\n"
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    "#include <inttypes.h>\n"
    "#include <sys/time.h>\n"
    "#include \"mquickjs.h\"\n"
    "#include \"mqjs.h\"\n"
    "#include \"mqjs_stdlib.h\"\n"
    "\n"
    "static uint8_t mem_buf[%zu];\n"
    "\n"
    "int main(int argc, const char **argv) {\n"
    "    JSContext *ctx;\n"
    "    JSValue val, arr, obj;\n"
    "    JSGCRef val_ref, arr_ref;\n"
    "    uint8_t *buf;\n"
    "    int i;\n"
    "    ctx = JS_NewContext(mem_buf, sizeof(mem_buf), &js_stdlib);\n"
    "    JS_SetLogFunc(ctx, js_log_func);\n"
    "    {\n"
    "        struct timeval tv;\n"
    "        gettimeofday(&tv, NULL);\n"
    "        JS_SetRandomSeed(ctx, ((uint64_t)tv.tv_sec << 32) ^ tv.tv_usec);\n"
    "    }\n"
    "    buf = malloc(bytecode_data_len + bytecode_payload_len);\n"
    "    memcpy(buf, bytecode_data, bytecode_data_len);\n"
    "    memcpy(buf + bytecode_data_len, bytecode_payload, bytecode_payload_len);\n"
    "    if (JS_RelocateBytecode(ctx, buf, bytecode_data_len + bytecode_payload_len)) {\n"
    "        fprintf(stderr, \"Could not relocate bytecode\\n\");\n"
    "        exit(1);\n"
    "    }\n"
    "    val = JS_LoadBytecode(ctx, buf);\n"
    "    if (JS_IsException(val)) {\n"
    "        dump_error(ctx);\n"
    "        exit(1);\n"
    "    }\n"
    "    if (argc > 1) {\n"
    "        JS_PUSH_VALUE(ctx, val);\n"
    "        arr = JS_NewArray(ctx, argc - 1);\n"
    "        JS_PUSH_VALUE(ctx, arr);\n"
    "        for(i = 1; i < argc; i++) {\n"
    "            JS_SetPropertyUint32(ctx, arr, i - 1, JS_NewString(ctx, argv[i]));\n"
    "        }\n"
    "        JS_POP_VALUE(ctx, arr);\n"
    "        obj = JS_GetGlobalObject(ctx);\n"
    "        JS_SetPropertyStr(ctx, obj, \"scriptArgs\", arr);\n"
    "        JS_POP_VALUE(ctx, val);\n"
    "    }\n"
    "    val = JS_Run(ctx, val);\n"
    "    if (JS_IsException(val)) {\n"
    "        dump_error(ctx);\n"
    "        exit(1);\n"
    "    }\n"
    "    run_timers(ctx);\n"
    "    JS_FreeContext(ctx);\n"
    "    return 0;\n"
    "}\n";

int main(int argc, const char **argv)
{
    int optind;
    const char *out_filename = "a.out";
    const char *input_filename = NULL;
    BOOL output_c_only = FALSE;
    BOOL force_32bit = FALSE;
    int parse_flags = 0;
    size_t mem_size = 16 << 20;

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
            if (opt == 'h' || !strcmp(longopt, "help")) {
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
            if (opt == 'c') {
                output_c_only = TRUE;
                continue;
            }
            if (opt == 'm' && !strcmp(arg, "32")) {
                force_32bit = TRUE;
                arg += strlen(arg);
                continue;
            }
            if (!strcmp(longopt, "no-column")) {
                parse_flags |= JS_EVAL_STRIP_COL;
                continue;
            }
            if (!strcmp(longopt, "memory-limit")) {
                char *p;
                double count;
                if (optind >= argc) {
                    fprintf(stderr, "expecting memory limit");
                    exit(1);
                }
                count = strtod(argv[optind++], &p);
                switch (tolower((unsigned char)*p)) {
                case 'g':
                    count *= 1024;
                case 'm':
                    count *= 1024;
                case 'k':
                    count *= 1024;
                default:
                    mem_size = (size_t)(count);
                    break;
                }
                continue;
            }
            fprintf(stderr, "mqjsc: unknown option\n");
            help();
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "expecting input filename\n");
        exit(1);
    }
    input_filename = argv[optind];

    uint8_t *mem_buf = malloc(mem_size);
    JSContext *ctx = JS_NewContext2(mem_buf, mem_size, &js_stdlib, TRUE);
    JS_SetLogFunc(ctx, js_log_func);

    int eval_len;
    char *eval_str = (char *)load_file(input_filename, &eval_len);
    JSValue val = JS_Parse(ctx, eval_str, eval_len, input_filename, parse_flags);
    free(eval_str);
    if (JS_IsException(val)) {
        dump_error(ctx);
        exit(1);
    }

    union {
        JSBytecodeHeader hdr;
#if JSW == 8
        JSBytecodeHeader32 hdr32;
#endif
    } hdr_buf;
    int hdr_len;
    const uint8_t *data_buf;
    uint32_t data_len;

#if JSW == 8
    if (force_32bit) {
        if (JS_PrepareBytecode64to32(ctx, &hdr_buf.hdr32, &data_buf, &data_len, val)) {
            fprintf(stderr, "Could not convert the bytecode from 64 to 32 bits\n");
            exit(1);
        }
        hdr_len = sizeof(JSBytecodeHeader32);
    } else
#endif
    {
        JS_PrepareBytecode(ctx, &hdr_buf.hdr, &data_buf, &data_len, val);
        JS_RelocateBytecode2(ctx, &hdr_buf.hdr, (uint8_t *)data_buf, data_len, 0, FALSE);
        hdr_len = sizeof(JSBytecodeHeader);
    }

    char c_filename[1024];
    if (output_c_only) {
        pstrcpy(c_filename, sizeof(c_filename), out_filename);
    } else {
        snprintf(c_filename, sizeof(c_filename), "mqjsc_%d_%ld.c", getpid(), (long)time(NULL));
    }

    FILE *f = fopen(c_filename, "w");
    if (!f) {
        perror(c_filename);
        exit(1);
    }

    fprintf(f, "#include <inttypes.h>\n\n");
    output_c(f, "bytecode_data", (uint8_t *)&hdr_buf, hdr_len);
    output_c(f, "bytecode_payload", data_buf, data_len);

    fprintf(f, main_c_template, mem_size);

    fclose(f);

    if (!output_c_only) {
        pid_t pid = fork();
        if (pid == 0) {
            const char *args[32];
            int n = 0;
            args[n++] = "gcc";
            args[n++] = "-O2";
            if (force_32bit)
                args[n++] = "-m32";
            args[n++] = "-o";
            args[n++] = out_filename;
            args[n++] = c_filename;
            args[n++] = "mqjs.c";
            args[n++] = "mquickjs.c";
            args[n++] = "dtoa.c";
            args[n++] = "libm.c";
            args[n++] = "cutils.c";
            args[n++] = "-lm";
            args[n++] = "-I.";
            args[n++] = NULL;
            execvp(args[0], (char **)args);
            perror("execvp");
            _exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Compilation failed\n");
                exit(1);
            }
        } else {
            perror("fork");
            exit(1);
        }
        unlink(c_filename);
    }

    JS_FreeContext(ctx);
    free(mem_buf);
    return 0;
}
