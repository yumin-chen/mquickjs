/* Example for mqjsc */

print("Welcome to MicroQuickJS Standalone!");

function greet(name) {
    print("Hello, " + name + "!");
}

if (scriptArgs && scriptArgs.length > 0) {
    for (var arg of scriptArgs) {
        greet(arg);
    }
} else {
    greet("World");
}

print("Memory limit: 16 MB (default in generated C wrapper)");
print("Current time: " + Date.now());

setTimeout(function() {
    print("This message is printed after 500ms");
}, 500);
