#!/bin/bash

# Simple mqjsc test script

MQJSC="./mqjsc"
EXIT_CODE=0

function test_compile_run {
    local name=$1
    local script=$2
    local expected=$3
    local args=$4

    echo "Testing $name..."
    $MQJSC -o "$name" "$script"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile $name"
        EXIT_CODE=1
        return
    fi

    local output=$("./$name" $args)
    if [[ "$output" == *"$expected"* ]]; then
        echo "Success: $name passed"
    else
        echo "Error: Unexpected output for $name"
        echo "Expected: $expected"
        echo "Got: $output"
        EXIT_CODE=1
    fi
    rm -f "$name"
}

# Test 1: Basic print
test_compile_run "test_hello" "examples/hello.js" "Hello from a standalone binary!" ""

# Test 2: Argument handling
test_compile_run "test_fib" "examples/fib.js" "fib(5) = 5" "5"

# Test 3: Standard language features
test_compile_run "test_builtin" "tests/test_builtin.js" "" ""

# Test 4: Array and object features
echo "var a = [1, 2, 3]; print(a.length); var o = {x: 1}; print(o.x);" > test_features.js
test_compile_run "test_features" "test_features.js" "3" ""
rm -f test_features.js

if [ $EXIT_CODE -eq 0 ]; then
    echo "All mqjsc tests passed!"
fi
exit $EXIT_CODE
