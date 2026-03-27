/*
 * Micro QuickJS standalone compiler
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include "cutils.h"
#include "mquickjs.h"
#include "mqjs.h"
#include "mqjs_stdlib.h"

static void help(void)
{
    printf("mqjsc version 1.0.0\n"
           "usage: mqjsc [options] [file]\n"
           "-h  --help            list options\n"
           "-o FILE               set the output filename (default = a.out)\n"
           "-c                    only output C source file\n"
           "-m32                  force 32 bit bytecode output\n");
    exit(1);
}

static const char main_c_template1[] =
    "#include <stdlib.h>\n"
    "#include <stdio.h>\n"
    "#include <inttypes.h>\n"
    "#include <string.h>\n"
    "#include \"mquickjs.h\"\n"
    "#include \"mqjs.h\"\n"
    "#include \"mqjs_stdlib.h\"\n"
    "\n"
    "static uint8_t bc_data[] = {\n";

static const char main_c_template2[] =
    "};\n"
    "\n"
    "int main(int argc, const char **argv)\n"
    "{\n"
    "    size_t mem_size = 16 << 20;\n"
    "    uint8_t *mem_buf = malloc(mem_size);\n"
    "    JSContext *ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);\n"
    "    JS_SetLogFunc(ctx, js_log_func);\n"
    "    JSValue val;\n"
    "    JSGCRef val_ref;\n"
    "    if (JS_RelocateBytecode(ctx, (uint8_t *)bc_data, sizeof(bc_data))) {\n"
    "        fprintf(stderr, \"Could not relocate bytecode\\n\");\n"
    "        return 1;\n"
    "    }\n"
    "    val = JS_LoadBytecode(ctx, bc_data);\n"
    "    if (JS_IsException(val)) {\n"
    "        dump_error(ctx);\n"
    "        return 1;\n"
    "    }\n"
    "    JS_PUSH_VALUE(ctx, val);\n"
    "    if (argc > 1) {\n"
    "        JSValue obj, arr;\n"
    "        JSGCRef arr_ref;\n"
    "        int i;\n"
    "        arr = JS_NewArray(ctx, argc - 1);\n"
    "        JS_AddGCRef(ctx, &arr_ref);\n"
    "        arr_ref.val = arr;\n"
    "        for(i = 1; i < argc; i++) {\n"
    "            JS_SetPropertyUint32(ctx, arr_ref.val, i - 1,\n"
    "                                 JS_NewString(ctx, argv[i]));\n"
    "        }\n"
    "        obj = JS_GetGlobalObject(ctx);\n"
    "        JS_SetPropertyStr(ctx, obj, \"scriptArgs\", arr_ref.val);\n"
    "        JS_DeleteGCRef(ctx, &arr_ref);\n"
    "    }\n"
    "    val = JS_Run(ctx, val_ref.val);\n"
    "    if (JS_IsException(val)) {\n"
    "        dump_error(ctx);\n"
    "        return 1;\n"
    "    }\n"
    "    run_timers(ctx);\n"
    "    JS_POP_VALUE(ctx, val);\n"
    "    JS_FreeContext(ctx);\n"
    "    free(mem_buf);\n"
    "    return 0;\n"
    "}\n";

static void output_c_wrapper(FILE *f, const uint8_t *bc_buf, uint32_t bc_len)
{
    uint32_t i;

    fputs(main_c_template1, f);
    for(i = 0; i < bc_len; i++) {
        fprintf(f, " 0x%02x,", bc_buf[i]);
        if ((i % 16) == 15)
            fprintf(f, "\n");
    }
    fputs(main_c_template2, f);
}

int main(int argc, char **argv)
{
    const char *out_filename = "a.out";
    const char *filename = NULL;
    int optind;
    BOOL force_32bit = FALSE;
    BOOL only_c = FALSE;
    size_t mem_size = 16 << 20;
    uint8_t *mem_buf;
    JSContext *ctx;
    char *eval_str;
    JSValue val;
    union {
        JSBytecodeHeader hdr;
#if JSW == 8
        JSBytecodeHeader32 hdr32;
#endif
    } hdr_buf;
    int hdr_len;
    const uint8_t *data_buf;
    uint32_t data_len;
    uint8_t *full_bc;
    FILE *f;
    char c_filename[1024];

    optind = 1;
    while (optind < argc && *argv[optind] == '-') {
        const char *arg = argv[optind] + 1;
        if (!strcmp(arg, "h") || !strcmp(arg, "-help") || !strcmp(arg, "?")) {
            help();
        } else if (!strcmp(arg, "o")) {
            optind++;
            if (optind >= argc) help();
            out_filename = argv[optind++];
        } else if (!strcmp(arg, "m32")) {
            force_32bit = TRUE;
            optind++;
        } else if (!strcmp(arg, "c")) {
            only_c = TRUE;
            optind++;
        } else {
            fprintf(stderr, "Unknown option: -%s\n", arg);
            help();
        }
    }

    if (optind >= argc) help();
    filename = argv[optind];

    mem_buf = malloc(mem_size);
    ctx = JS_NewContext2(mem_buf, mem_size, &js_stdlib, TRUE);
    JS_SetLogFunc(ctx, js_log_func);

    eval_str = (char *)load_file(filename, NULL);
    if (!eval_str) {
        fprintf(stderr, "Could not load %s\n", filename);
        exit(1);
    }

    val = JS_Parse(ctx, eval_str, strlen(eval_str), filename, 0);
    free(eval_str);
    if (JS_IsException(val)) {
        dump_error(ctx);
        exit(1);
    }

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

    full_bc = malloc(hdr_len + data_len);
    memcpy(full_bc, &hdr_buf, hdr_len);
    memcpy(full_bc + hdr_len, data_buf, data_len);

    if (only_c) {
        f = fopen(out_filename, "w");
        if (!f) {
            perror(out_filename);
            exit(1);
        }
        output_c_wrapper(f, full_bc, hdr_len + data_len);
        fclose(f);
    } else {
        snprintf(c_filename, sizeof(c_filename), "%s.c", out_filename);
        f = fopen(c_filename, "w");
        if (!f) {
            perror(c_filename);
            exit(1);
        }
        output_c_wrapper(f, full_bc, hdr_len + data_len);
        fclose(f);

        char cmd[2048];
        char *path = getcwd(NULL, 0);
        snprintf(cmd, sizeof(cmd), "gcc -O2 -I%s -o %s %s %s/libmquickjs.a -lm", path, out_filename, c_filename, path);
        free(path);
        // printf("%s\n", cmd);
        if (system(cmd) != 0) {
            fprintf(stderr, "Compilation failed\n");
            exit(1);
        }
        unlink(c_filename);
    }

    free(full_bc);
    JS_FreeContext(ctx);
    free(mem_buf);
    return 0;
}
