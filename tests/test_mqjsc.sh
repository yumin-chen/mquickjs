#!/bin/sh

# Test mqjsc compiler

set -e

MQJSC="./mqjsc"

# Test 1: Simple print
echo "print('hello');" > tests/test1.js
$MQJSC -o tests/test1 tests/test1.js
res=`./tests/test1`
if [ "$res" != "hello" ]; then
    echo "Test 1 failed: $res"
    exit 1
fi
echo "Test 1 passed"

# Test 2: scriptArgs
echo "print(scriptArgs.join(' '));" > tests/test2.js
$MQJSC -o tests/test2 tests/test2.js
res=`./tests/test2 arg1 arg2`
if [ "$res" != "arg1 arg2" ]; then
    echo "Test 2 failed: $res"
    exit 1
fi
echo "Test 2 passed"

# Test 3: timers
echo "setTimeout(function() { print('timeout'); }, 100);" > tests/test3.js
$MQJSC -o tests/test3 tests/test3.js
res=`./tests/test3`
if [ "$res" != "timeout" ]; then
    echo "Test 3 failed: $res"
    exit 1
fi
echo "Test 3 passed"

# Cleanup
rm -f tests/test1.js tests/test1 tests/test2.js tests/test2 tests/test3.js tests/test3
