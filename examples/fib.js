function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
var n = scriptArgs.length > 1 ? parseInt(scriptArgs[1]) : 10;
print('fib(' + n + ') = ' + fib(n));
