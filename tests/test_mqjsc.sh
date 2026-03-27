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
RES=$(./args_test foo bar baz)
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

# Test 4: Comprehensive features
echo "Test 4: Comprehensive features..."
$MQJSC -o features_test tests/test_compiler_features.js
./features_test
rm features_test
echo "Test 4 Passed: Comprehensive features"

# Test 5: Existing closure test
echo "Test 5: Existing closure test..."
$MQJSC -o closure_test tests/test_closure.js
./closure_test
rm closure_test
echo "Test 5 Passed: Existing closure test"

# Test 6: Existing language test
echo "Test 6: Existing language test..."
$MQJSC -o language_test tests/test_language.js
./language_test
rm language_test
echo "Test 6 Passed: Existing language test"

# Test 7: Example script
echo "Test 7: Example script..."
$MQJSC -o hello_mqjsc examples/hello_mqjsc.js
./hello_mqjsc Alice Bob > /dev/null
rm hello_mqjsc
echo "Test 7 Passed: Example script"

echo "All mqjsc tests passed!"
