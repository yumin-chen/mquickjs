#!/bin/bash

# mqjsc test suite

set -e

MQJSC="./mqjsc"
EXAMPLES="examples"
TEST_TMP="test_tmp"

mkdir -p $TEST_TMP

echo "--- Basic compilation and execution ---"
$MQJSC -o $TEST_TMP/hello $EXAMPLES/hello.js
$TEST_TMP/hello | grep -q "Hello from MQuickJS standalone binary!"
echo "OK"

echo "--- Custom output name (-o) ---"
$MQJSC -o $TEST_TMP/my_fib $EXAMPLES/fib.js
$TEST_TMP/my_fib | grep -q "fib(20) = 6765"
echo "OK"

echo "--- C source generation only (-c) ---"
$MQJSC -c -o $TEST_TMP/hello.c $EXAMPLES/hello.js
[ -f $TEST_TMP/hello.c ]
grep -q "bytecode_data" $TEST_TMP/hello.c
echo "OK"

echo "--- Memory limit (--memory-limit) ---"
# Test with a very small memory limit that should fail for fib(20) if it's too small,
# but here we just ensure the flag is accepted and binary runs.
$MQJSC --memory-limit 1M -o $TEST_TMP/fib_1m $EXAMPLES/fib.js
$TEST_TMP/fib_1m | grep -q "fib(20) = 6765"
echo "OK"

echo "--- No column info (--no-column) ---"
$MQJSC --no-column -o $TEST_TMP/hello_nocol $EXAMPLES/hello.js
$TEST_TMP/hello_nocol | grep -q "Hello from MQuickJS standalone binary!"
echo "OK"

echo "--- scriptArgs consistency ---"
$MQJSC -o $TEST_TMP/args $EXAMPLES/args.js
OUTPUT=$($TEST_TMP/args val1 val2)
echo "$OUTPUT" | grep -q "Argument 0: val1"
echo "$OUTPUT" | grep -q "Argument 1: val2"
echo "$OUTPUT" | grep -v -q "Argument 2"
echo "OK"

echo "--- Timers and event loop ---"
$MQJSC -o $TEST_TMP/timers $EXAMPLES/timers.js
$TEST_TMP/timers | grep -q "Timer 3 (1000ms) expired. Exiting."
echo "OK"

# Cleanup
rm -rf $TEST_TMP

echo "All mqjsc tests passed!"
