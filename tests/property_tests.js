import * as fc from 'fast-check';
import { engine } from './microquickjs.js';
import assert from 'node:assert';
import { readFile } from 'node:fs/promises';

// Feature: microquickjs-wasi-component, Property 1: eval of valid JS returns ok
async function testProperty1() {
    console.log("Running Property 1...");
    await fc.assert(fc.asyncProperty(
        fc.integer({ min: -1000, max: 1000 }),
        fc.integer({ min: -1000, max: 1000 }),
        async (a, b) => {
            const result = engine.eval(`${a} + ${b}`);
            assert(result.tag === 'ok', `Expected ok, got ${result.tag}: ${result.val}`);
            assert(result.val === String(a + b));
        }
    ), { numRuns: 100 });
}

// Feature: microquickjs-wasi-component, Property 2: eval of throwing JS returns err
async function testProperty2() {
    console.log("Running Property 2...");
    await fc.assert(fc.asyncProperty(
        fc.string({ minLength: 1, maxLength: 50 }).filter(s => /^[a-zA-Z0-9 ]+$/.test(s)),
        async (msg) => {
            const result = engine.eval(`throw new Error(${JSON.stringify(msg)})`);
            assert(result.tag === 'err', `Expected err, got ${result.tag}`);
            assert(result.val.length > 0);
            assert(result.val.includes(msg));
        }
    ), { numRuns: 100 });
}

// Feature: microquickjs-wasi-component, Property 3: global state persists across eval calls
async function testProperty3() {
    console.log("Running Property 3...");
    await fc.assert(fc.asyncProperty(
        fc.string({ minLength: 1, maxLength: 20 }).filter(s => /^[a-zA-Z_][a-zA-Z0-9_]*$/.test(s)),
        fc.integer({ min: 0, max: 999999 }),
        async (name, value) => {
            engine.eval(`var ${name} = ${value}`);
            const result = engine.eval(name);
            assert(result.tag === 'ok');
            assert(result.val === String(value));
        }
    ), { numRuns: 100 });
}

// Feature: microquickjs-wasi-component, Property 4: new-T then is-T returns true
async function testProperty4() {
    console.log("Running Property 4...");
    await fc.assert(fc.asyncProperty(
        fc.integer({ min: -(2**31), max: 2**31 - 1 }),
        async (n) => {
            const v = engine.newInt32(n);
            assert(v.isInt() === true);
            assert(v.isBool() === false);
            assert(v.isNull() === false);
            assert(v.isUndefined() === false);
        }
    ), { numRuns: 100 });
}

// Feature: microquickjs-wasi-component, Property 5: new-int32 then to-int32 round-trips
async function testProperty5() {
    console.log("Running Property 5...");
    await fc.assert(fc.asyncProperty(
        fc.integer({ min: -(2**31), max: 2**31 - 1 }),
        async (n) => {
            const v = engine.newInt32(n);
            assert(v.toInt32() === n);
        }
    ), { numRuns: 100 });
}

// Feature: microquickjs-wasi-component, Property 6: set-property then get-property round-trips
async function testProperty6() {
    console.log("Running Property 6...");
    await fc.assert(fc.asyncProperty(
        fc.string({ minLength: 1, maxLength: 20 }).filter(s => /^[a-zA-Z_][a-zA-Z0-9_]*$/.test(s)),
        fc.integer({ min: 0, max: 999999 }),
        async (key, value) => {
            const obj = engine.newObject();
            const val = engine.newInt32(value);
            obj.setProperty(key, val);
            const got = obj.getProperty(key);
            assert(got.toInt32() === value);
        }
    ), { numRuns: 100 });
}

// Feature: microquickjs-wasi-component, Property 7: function call produces correct result
async function testProperty7() {
    console.log("Running Property 7...");
    await fc.assert(fc.asyncProperty(
        fc.integer({ min: 0, max: 10000 }),
        async (n) => {
            engine.eval('function double(x) { return x * 2; }');
            const global = engine.getGlobalObject();
            const fn = global.getProperty('double');
            const arg = engine.newInt32(n);
            const result = fn.call([arg]);
            assert(result.toInt32() === n * 2);
        }
    ), { numRuns: 100 });
}

async function runTests() {
    try {
        await testProperty1();
        await testProperty2();
        await testProperty3();
        await testProperty4();
        await testProperty5();
        await testProperty6();
        await testProperty7();
        console.log("All property tests passed!");
    } catch (e) {
        console.error("Test failed!");
        console.error(e);
        process.exit(1);
    }
}

runTests();
