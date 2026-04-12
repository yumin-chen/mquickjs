#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "mquickjs.h"

// cabi_realloc is provided by wit-bindgen's generated microquickjs.c.
void *cabi_realloc(void *ptr, size_t old_size, size_t align, size_t new_size);

// WASI shim stubs
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
    { return JS_UNDEFINED; }

#include "generated/microquickjs.h"
#include "build/mqjs_stdlib.h"

static uint8_t s_mem[4 * 1024 * 1024];  // 4 MiB arena
static JSContext *s_ctx = NULL;

static void ensure_context(void) {
    if (s_ctx) return;
    s_ctx = JS_NewContext(s_mem, sizeof(s_mem), &js_stdlib);
}

struct exports_local_microquickjs_engine_js_value_t {
    JSValue val;
    JSGCRef root;
};

static exports_local_microquickjs_engine_own_js_value_t
make_own_value(JSValue val) {
    exports_local_microquickjs_engine_js_value_t *rep = malloc(sizeof(*rep));
    rep->val = val;
    JS_AddGCRef(s_ctx, &rep->root);
    rep->root.val = val;
    return exports_local_microquickjs_engine_js_value_new(rep);
}

void exports_local_microquickjs_engine_js_value_destructor(
    exports_local_microquickjs_engine_js_value_t *rep)
{
    JS_DeleteGCRef(s_ctx, &rep->root);
    free(rep);
}

bool exports_local_microquickjs_engine_eval(
    microquickjs_string_t *code,
    microquickjs_string_t *ret,
    microquickjs_string_t *err)
{
    ensure_context();
    JSValue val = JS_Eval(s_ctx,
                          (const char *)code->ptr,
                          code->len,
                          "<eval>",
                          JS_EVAL_RETVAL);
    size_t len;
    JSCStringBuf buf;

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(s_ctx);
        const char *cstr = JS_ToCStringLen(s_ctx, &len, exc, &buf);
        if (!cstr) {
            static const char unknown[] = "Error: unknown exception";
            err->ptr = cabi_realloc(NULL, 0, 1, sizeof(unknown) - 1);
            memcpy(err->ptr, unknown, sizeof(unknown) - 1);
            err->len = sizeof(unknown) - 1;
        } else {
            err->ptr = cabi_realloc(NULL, 0, 1, len);
            memcpy(err->ptr, cstr, len);
            err->len = len;
        }
        return false;
    }

    const char *cstr = JS_ToCStringLen(s_ctx, &len, val, &buf);
    if (!cstr) {
        static const char undef[] = "undefined";
        ret->ptr = cabi_realloc(NULL, 0, 1, sizeof(undef) - 1);
        memcpy(ret->ptr, undef, sizeof(undef) - 1);
        ret->len = sizeof(undef) - 1;
    } else {
        ret->ptr = cabi_realloc(NULL, 0, 1, len);
        memcpy(ret->ptr, cstr, len);
        ret->len = len;
    }
    return true;
}

bool exports_local_microquickjs_engine_method_js_value_is_int(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    return JS_IsInt(self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_bool(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    return JS_IsBool(self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_null(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    return JS_IsNull(self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_undefined(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    return JS_IsUndefined(self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_exception(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    return JS_IsException(self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_number(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    ensure_context();
    return JS_IsNumber(s_ctx, self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_string(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    ensure_context();
    return JS_IsString(s_ctx, self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_error(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    ensure_context();
    return JS_IsError(s_ctx, self->val);
}

bool exports_local_microquickjs_engine_method_js_value_is_function(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    ensure_context();
    return JS_IsFunction(s_ctx, self->val);
}

void exports_local_microquickjs_engine_method_js_value_to_string(
    exports_local_microquickjs_engine_borrow_js_value_t self,
    microquickjs_string_t *ret)
{
    ensure_context();
    size_t len;
    JSCStringBuf buf;
    const char *cstr = JS_ToCStringLen(s_ctx, &len, self->val, &buf);
    if (!cstr) {
        ret->ptr = NULL;
        ret->len = 0;
    } else {
        ret->ptr = cabi_realloc(NULL, 0, 1, len);
        memcpy(ret->ptr, cstr, len);
        ret->len = len;
    }
}

int32_t exports_local_microquickjs_engine_method_js_value_to_int32(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    ensure_context();
    int32_t res = 0;
    JS_ToInt32(s_ctx, &res, self->val);
    return res;
}

double exports_local_microquickjs_engine_method_js_value_to_float64(
    exports_local_microquickjs_engine_borrow_js_value_t self)
{
    ensure_context();
    double res = 0;
    JS_ToNumber(s_ctx, &res, self->val);
    return res;
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_method_js_value_get_property(
    exports_local_microquickjs_engine_borrow_js_value_t self,
    microquickjs_string_t *name)
{
    ensure_context();
    char *cname = malloc(name->len + 1);
    memcpy(cname, name->ptr, name->len);
    cname[name->len] = '\0';
    JSValue res = JS_GetPropertyStr(s_ctx, self->val, cname);
    free(cname);
    return make_own_value(res);
}

void exports_local_microquickjs_engine_method_js_value_set_property(
    exports_local_microquickjs_engine_borrow_js_value_t self,
    microquickjs_string_t *name,
    exports_local_microquickjs_engine_borrow_js_value_t val)
{
    ensure_context();
    char *cname = malloc(name->len + 1);
    memcpy(cname, name->ptr, name->len);
    cname[name->len] = '\0';
    JS_SetPropertyStr(s_ctx, self->val, cname, val->val);
    free(cname);
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_method_js_value_call(
    exports_local_microquickjs_engine_borrow_js_value_t self,
    exports_local_microquickjs_engine_list_borrow_js_value_t *args)
{
    ensure_context();
    // MicroQuickJS calling convention: push args, then func, then this.
    // JS_Call(ctx, argc) then pops them.
    if (JS_StackCheck(s_ctx, args->len + 3)) {
        return make_own_value(JS_EXCEPTION);
    }
    for (size_t i = 0; i < args->len; i++) {
        JS_PushArg(s_ctx, args->ptr[i]->val);
    }
    JS_PushArg(s_ctx, self->val);
    JS_PushArg(s_ctx, JS_UNDEFINED); // default 'this' to undefined
    JSValue res = JS_Call(s_ctx, (int)args->len);
    return make_own_value(res);
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_new_int32(int32_t val)
{
    ensure_context();
    return make_own_value(JS_NewInt32(s_ctx, val));
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_new_float64(double val)
{
    ensure_context();
    return make_own_value(JS_NewFloat64(s_ctx, val));
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_new_bool(bool val)
{
    ensure_context();
    return make_own_value(JS_NewBool(val));
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_new_string(microquickjs_string_t *val)
{
    ensure_context();
    return make_own_value(JS_NewStringLen(s_ctx, (const char *)val->ptr, val->len));
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_new_object(void)
{
    ensure_context();
    return make_own_value(JS_NewObject(s_ctx));
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_new_array(void)
{
    ensure_context();
    return make_own_value(JS_NewArray(s_ctx, 0));
}

exports_local_microquickjs_engine_own_js_value_t
exports_local_microquickjs_engine_get_global_object(void)
{
    ensure_context();
    return make_own_value(JS_GetGlobalObject(s_ctx));
}
