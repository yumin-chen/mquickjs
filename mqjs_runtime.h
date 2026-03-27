#ifndef MQJS_RUNTIME_H
#define MQJS_RUNTIME_H

#include "mquickjs.h"

void mqjs_add_runtime_functions(JSContext *ctx);
void run_timers(JSContext *ctx);
void dump_error(JSContext *ctx);
void js_log_func(void *opaque, const void *buf, size_t buf_len);
int64_t get_time_ms(void);
uint8_t *load_file(const char *filename, int *plen);

/* these are the functions that the standard library expects */
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#endif /* MQJS_RUNTIME_H */
