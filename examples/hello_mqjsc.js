/*
  MicroQuickJS (MQuickJS) standalone compilation example.
  Usage:
    ./mqjsc -o hello_mqjsc examples/hello_mqjsc.js
    ./hello_mqjsc Alice Bob
*/

print("Hello from a standalone MicroQuickJS binary!");

if (typeof scriptArgs !== 'undefined' && scriptArgs.length > 0) {
    print("Arguments provided:");
    for (var i = 0; i < scriptArgs.length; i++) {
        print("  - " + scriptArgs[i]);
    }
} else {
    print("No arguments provided.");
}

function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

print("Fibonacci(10) =", fib(10));
