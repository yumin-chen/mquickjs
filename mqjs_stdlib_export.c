#include "mquickjs.h"
#include "mqjs_runtime.h"
#include "mqjs_stdlib.h"

const JSSTDLibraryDef *mqjs_get_stdlib(void)
{
    return &js_stdlib;
}
