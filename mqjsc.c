#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>

#include "cutils.h"
#include "mquickjs.h"
#include "mqjs_runtime.h"

static void help(void)
{
    printf("MicroQuickJS Compiler\n"
           "usage: mqjsc [options] [file]\n"
           "-h  --help            list options\n"
           "-o FILE               set the output filename (default = a.out)\n"
           "-c                    only generate C source file\n"
           "-m32                  force 32 bit bytecode output\n"
           );
    exit(1);
}

static void output_c_string(FILE *f, const uint8_t *buf, size_t len, const char *name)
{
    size_t i;
    fprintf(f, "const uint8_t %s[%zu] = {", name, len);
    for(i = 0; i < len; i++) {
        if (i % 16 == 0)
            fprintf(f, "\n   ");
        fprintf(f, " 0x%02x,", buf[i]);
    }
    fprintf(f, "\n};\n\n");
}

static void compile_file(const char *filename, const char *out_filename,
                         BOOL only_c, BOOL force_32bit)
{
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
    FILE *f;
    char c_filename[1024];
    size_t mem_size = 16 << 20;
    extern const JSSTDLibraryDef js_stdlib;

    mem_buf = malloc(mem_size);
    ctx = JS_NewContext2(mem_buf, mem_size, &js_stdlib, TRUE);
    mqjs_add_runtime_functions(ctx);

    eval_str = (char *)load_file(filename, NULL);

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
        /* Relocate to zero to have a deterministic output */
        JS_RelocateBytecode2(ctx, &hdr_buf.hdr, (uint8_t *)data_buf, data_len, 0, FALSE);
        hdr_len = sizeof(JSBytecodeHeader);
    }

    if (only_c) {
        pstrcpy(c_filename, sizeof(c_filename), out_filename);
    } else {
        snprintf(c_filename, sizeof(c_filename), "/tmp/mqjsc%d.c", getpid());
    }

    f = fopen(c_filename, "w");
    if (!f) {
        perror(c_filename);
        exit(1);
    }

    fprintf(f, "#include <stdlib.h>\n"
               "#include <stdio.h>\n"
               "#include <string.h>\n"
               "#include \"mquickjs.h\"\n"
               "#include \"mqjs_runtime.h\"\n\n");

    output_c_string(f, (uint8_t *)&hdr_buf, hdr_len, "mqjs_bytecode_hdr");
    output_c_string(f, data_buf, data_len, "mqjs_bytecode_data");

    fprintf(f, "int main(int argc, const char **argv)\n"
               "{\n"
               "    uint8_t *mem_buf, *bc_buf;\n"
               "    JSContext *ctx;\n"
               "    JSValue val;\n"
               "    size_t mem_size = 16 << 20;\n"
               "    extern const JSSTDLibraryDef js_stdlib;\n"
               "\n"
               "    mem_buf = malloc(mem_size);\n"
               "    ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);\n"
               "    mqjs_add_runtime_functions(ctx);\n"
               "\n"
               "    bc_buf = malloc(sizeof(mqjs_bytecode_hdr) + sizeof(mqjs_bytecode_data));\n"
               "    memcpy(bc_buf, mqjs_bytecode_hdr, sizeof(mqjs_bytecode_hdr));\n"
               "    memcpy(bc_buf + sizeof(mqjs_bytecode_hdr), mqjs_bytecode_data, sizeof(mqjs_bytecode_data));\n"
               "\n"
               "    if (JS_RelocateBytecode(ctx, bc_buf, sizeof(mqjs_bytecode_hdr) + sizeof(mqjs_bytecode_data))) {\n"
               "        fprintf(stderr, \"Could not relocate bytecode\\n\");\n"
               "        exit(1);\n"
               "    }\n"
               "    val = JS_LoadBytecode(ctx, bc_buf);\n"
               "\n"
               "    if (argc > 0) {\n"
               "        JSValue obj, *arr;\n"
               "        JSGCRef val_ref, arr_ref;\n"
               "        int i;\n"
               "        JS_PUSH_VALUE(ctx, val);\n"
               "        arr = JS_PushGCRef(ctx, &arr_ref);\n"
               "        *arr = JS_NewArray(ctx, argc);\n"
               "        if (JS_IsException(*arr))\n"
               "            goto fail_args;\n"
               "        for(i = 0; i < argc; i++) {\n"
               "            JS_SetPropertyUint32(ctx, *arr, i, JS_NewString(ctx, argv[i]));\n"
               "        }\n"
               "        obj = JS_GetGlobalObject(ctx);\n"
               "        JS_SetPropertyStr(ctx, obj, \"scriptArgs\", *arr);\n"
               "    fail_args:\n"
               "        JS_PopGCRef(ctx, &arr_ref);\n"
               "        JS_POP_VALUE(ctx, val);\n"
               "    }\n"
               "\n"
               "    val = JS_Run(ctx, val);\n"
               "    if (JS_IsException(val)) {\n"
               "        dump_error(ctx);\n"
               "        exit(1);\n"
               "    }\n"
               "    run_timers(ctx);\n"
               "    JS_FreeContext(ctx);\n"
               "    free(mem_buf);\n"
               "    free(bc_buf);\n"
               "    return 0;\n"
               "}\n");
    fclose(f);

    if (!only_c) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "gcc -I. %s -o %s %s libmquickjs.a -lm",
                 c_filename, out_filename, force_32bit ? "-m32" : "");
        if (system(cmd) != 0) {
            fprintf(stderr, "Compilation failed\n");
            exit(1);
        }
        unlink(c_filename);
    }

    JS_FreeContext(ctx);
    free(mem_buf);
}

int main(int argc, const char **argv)
{
    int optind;
    const char *filename = NULL;
    const char *out_filename = "a.out";
    BOOL only_c = FALSE;
    BOOL force_32bit = FALSE;

    optind = 1;
    while (optind < argc && *argv[optind] == '-') {
        const char *arg = argv[optind] + 1;
        if (!strcmp(arg, "h") || !strcmp(arg, "-help")) {
            help();
        } else if (!strcmp(arg, "o")) {
            optind++;
            if (optind >= argc) help();
            out_filename = argv[optind++];
        } else if (!strcmp(arg, "c")) {
            only_c = TRUE;
            optind++;
        } else if (!strcmp(arg, "m32")) {
            force_32bit = TRUE;
            optind++;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[optind]);
            help();
        }
    }

    if (optind >= argc) help();
    filename = argv[optind];

    compile_file(filename, out_filename, only_c, force_32bit);

    return 0;
}
