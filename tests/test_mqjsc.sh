#!/bin/bash

# Simple test script for mqjsc
set -e

# Build the compiler first
make mqjsc

MQJSC=./mqjsc

# Test 1: Simple hello world
echo "print('hello');" > hello_test.js
$MQJSC -o hello_test hello_test.js
RES=$(./hello_test)
if [ "$RES" != "hello" ]; then
    echo "Test 1 Failed: expected 'hello', got '$RES'"
    exit 1
fi
rm hello_test.js hello_test
echo "Test 1 Passed: Simple hello world"

# Test 2: scriptArgs
echo "print(scriptArgs.join(' '));" > args_test.js
$MQJSC -o args_test args_test.js
RES=$($MQJSC -o args_test args_test.js && ./args_test foo bar baz)
if [ "$RES" != "foo bar baz" ]; then
    echo "Test 2 Failed: expected 'foo bar baz', got '$RES'"
    exit 1
fi
rm args_test.js args_test
echo "Test 2 Passed: scriptArgs handling"

# Test 3: eval expression
$MQJSC -o eval_test -e "print(1+2+3)"
RES=$(./eval_test)
if [ "$RES" != "6" ]; then
    echo "Test 3 Failed: expected '6', got '$RES'"
    exit 1
fi
rm eval_test
echo "Test 3 Passed: eval expression compilation"

echo "All mqjsc tests passed!"
