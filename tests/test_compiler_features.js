/* Comprehensive tests for MicroQuickJS compiler */

function assert(condition, message) {
    if (!condition) {
        print("Assertion failed: " + message);
        throw Error(message);
    }
}

// 1. Closures
function test_closures() {
    function makeAdder(x) {
        return function(y) {
            return x + y;
        };
    }
    var add5 = makeAdder(5);
    assert(add5(10) === 15, "add5(10) === 15");

    var counter = (function() {
        var count = 0;
        return {
            inc: function() { return ++count; },
            get: function() { return count; }
        };
    })();
    assert(counter.get() === 0, "counter.get() === 0");
    assert(counter.inc() === 1, "counter.inc() === 1");
    assert(counter.inc() === 2, "counter.inc() === 2");
    assert(counter.get() === 2, "counter.get() === 2");
}

// 2. RegExp
function test_regexp() {
    var re = /a(b+)c/i;
    var m = re.exec("ABBC");
    assert(m !== null, "RegExp match");
    assert(m[0] === "ABBC", "RegExp match group 0");
    assert(m[1] === "BB", "RegExp match group 1");

    assert("hello world".replace(/o/g, "0") === "hell0 w0rld", "RegExp replace");
    assert(/abc/.test("abcdef") === true, "RegExp test true");
    assert(/abc/.test("abdef") === false, "RegExp test false");
}

// 3. Arrays and Typed Arrays
function test_arrays() {
    var a = [1, 2, 3];
    assert(a.map(function(x) { return x * 2; }).join(",") === "2,4,6", "Array map");
    assert(a.filter(function(x) { return x > 1; }).length === 2, "Array filter");
    assert(a.reduce(function(acc, x) { return acc + x; }, 0) === 6, "Array reduce");

    var ta = new Uint8Array([10, 20, 30]);
    assert(ta[1] === 20, "TypedArray index");
    ta[0] = 256; // wraps to 0
    assert(ta[0] === 0, "TypedArray wrap");
}

// 4. Objects and Prototypes
function test_objects() {
    var proto = {
        sayHello: function() { return "Hello " + this.name; }
    };
    var obj = Object.create(proto);
    obj.name = "World";
    assert(obj.sayHello() === "Hello World", "Object prototype");

    var keys = Object.keys({a: 1, b: 2});
    assert(keys.length === 2, "Object.keys length");
    assert(keys.indexOf("a") !== -1, "Object.keys contains a");
}

// 5. JSON
function test_json() {
    var obj = { a: 1, b: [2, 3], c: "s" };
    var s = JSON.stringify(obj);
    var obj2 = JSON.parse(s);
    assert(obj2.a === 1, "JSON parse a");
    assert(obj2.b[1] === 3, "JSON parse b[1]");
    assert(obj2.c === "s", "JSON parse c");
}

print("Running comprehensive compiler tests...");
test_closures();
test_regexp();
test_arrays();
test_objects();
test_json();
print("All compiler feature tests passed!");
