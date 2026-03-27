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

#include "cutils.h"
#include "mquickjs.h"
#include "mqjs_runtime.h"

#define COLOR_NONE           "\033[0m"
#define COLOR_BRIGHT_RED     "\033[1;31m"

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;

    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            fwrite(str, 1, len, stdout);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JS_GC(ctx);
    return JS_UNDEFINED;
}

#if defined(__linux__) || defined(__APPLE__)
int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}
#else
int64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}
#endif

JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, get_time_ms());
}

/* load a script */
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    const char *filename;
    JSCStringBuf buf_str;
    uint8_t *buf;
    int buf_len;
    JSValue ret;

    filename = JS_ToCString(ctx, argv[0], &buf_str);
    if (!filename)
        return JS_EXCEPTION;
    buf = load_file(filename, &buf_len);

    ret = JS_Eval(ctx, (const char *)buf, buf_len, filename, 0);
    free(buf);
    return ret;
}

/* timers */
typedef struct {
    BOOL allocated;
    JSGCRef func;
    int64_t timeout; /* in ms */
} JSTimer;

#define MAX_TIMERS 16

static JSTimer js_timer_list[MAX_TIMERS];

JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSTimer *th;
    int delay, i;
    JSValue *pfunc;

    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "not a function");
    if (JS_ToInt32(ctx, &delay, argv[1]))
        return JS_EXCEPTION;
    for(i = 0; i < MAX_TIMERS; i++) {
        th = &js_timer_list[i];
        if (!th->allocated) {
            pfunc = JS_AddGCRef(ctx, &th->func);
            *pfunc = argv[0];
            th->timeout = get_time_ms() + delay;
            th->allocated = TRUE;
            return JS_NewInt32(ctx, i);
        }
    }
    return JS_ThrowInternalError(ctx, "too many timers");
}

JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int timer_id;
    JSTimer *th;

    if (JS_ToInt32(ctx, &timer_id, argv[0]))
        return JS_EXCEPTION;
    if (timer_id >= 0 && timer_id < MAX_TIMERS) {
        th = &js_timer_list[timer_id];
        if (th->allocated) {
            JS_DeleteGCRef(ctx, &th->func);
            th->allocated = FALSE;
        }
    }
    return JS_UNDEFINED;
}

void run_timers(JSContext *ctx)
{
    int64_t min_delay, delay, cur_time;
    BOOL has_timer;
    int i;
    JSTimer *th;
    struct timespec ts;

    for(;;) {
        min_delay = 1000;
        cur_time = get_time_ms();
        has_timer = FALSE;
        for(i = 0; i < MAX_TIMERS; i++) {
            th = &js_timer_list[i];
            if (th->allocated) {
                has_timer = TRUE;
                delay = th->timeout - cur_time;
                if (delay <= 0) {
                    JSValue ret;
                    /* the timer expired */
                    if (JS_StackCheck(ctx, 2))
                        goto fail;
                    JS_PushArg(ctx, th->func.val); /* func name */
                    JS_PushArg(ctx, JS_NULL); /* this */

                    JS_DeleteGCRef(ctx, &th->func);
                    th->allocated = FALSE;

                    ret = JS_Call(ctx, 0);
                    if (JS_IsException(ret)) {
                    fail:
                        dump_error(ctx);
                        exit(1);
                    }
                    min_delay = 0;
                    break;
                } else if (delay < min_delay) {
                    min_delay = delay;
                }
            }
        }
        if (!has_timer)
            break;
        if (min_delay > 0) {
            ts.tv_sec = min_delay / 1000;
            ts.tv_nsec = (min_delay % 1000) * 1000000;
            nanosleep(&ts, NULL);
        }
    }
}

uint8_t *load_file(const char *filename, int *plen)
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
        fprintf(stderr, "could not allocate memory\n");
        exit(1);
    }
    if (fread(buf, 1, buf_len, f) != buf_len) {
        fprintf(stderr, "could not read file\n");
        exit(1);
    }
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}

static int js_log_err_flag;

void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    if (fwrite(buf, 1, buf_len, js_log_err_flag ? stderr : stdout) != buf_len) {
        /* ignore write errors */
    }
}

void dump_error(JSContext *ctx)
{
    JSValue obj;
    obj = JS_GetException(ctx);
    fprintf(stderr, "%s", COLOR_BRIGHT_RED);
    js_log_err_flag++;
    JS_PrintValueF(ctx, obj, JS_DUMP_LONG);
    js_log_err_flag--;
    fprintf(stderr, "%s\n", COLOR_NONE);
}

void mqjs_add_runtime_functions(JSContext *ctx)
{
    JS_SetLogFunc(ctx, js_log_func);
}
