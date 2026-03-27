if (typeof scriptArgs !== "undefined") {
    print("Arguments provided: " + scriptArgs.length);
    for (var i = 0; i < scriptArgs.length; i++) {
        print("Argument " + i + ": " + scriptArgs[i]);
    }
} else {
    print("No arguments provided (scriptArgs is undefined).");
}
