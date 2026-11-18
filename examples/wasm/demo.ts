// examples/wasm/demo.ts
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Uses the WebAssembly module from TypeScript. It is run by ctest with node
// (>= 22.18 strips the types itself):  node demo.ts
// Type checking, if you have TypeScript: tsc --noEmit --module nodenext \
//     --allowImportingTsExtensions --strict --target es2022 demo.ts

import assert from 'node:assert/strict';
import createModule from './beman_wasm.mjs';

// What module.cpp exports. `--emit-tsd` generates beman_wasm.d.ts with these
// functions typed as `any` because embind doesn't know about promises.
interface BemanWasm {
    sum(a: Promise<number>, b: Promise<number>, signal?: AbortSignal): Promise<number>;
    all<A, B>(a: Promise<A>, b: Promise<B>, signal?: AbortSignal): Promise<[A, B]>;
    delay<T>(ms: number, value: T, signal?: AbortSignal): Promise<T>;
    abortable<T>(promise: Promise<T>, signal: AbortSignal): Promise<T>;
}

const wasm = (await createModule()) as unknown as BemanWasm;

const later = <T>(ms: number, value: T): Promise<T> => new Promise((resolve) => setTimeout(() => resolve(value), ms));
const failing = (ms: number, message: string): Promise<never> =>
    new Promise((_, reject) => setTimeout(() => reject(new Error(message)), ms));

async function rejection(promise: Promise<unknown>): Promise<Error> {
    try {
        await promise;
    } catch (error) {
        return error as Error;
    }
    assert.fail('the promise should have been rejected');
}

const start = Date.now();

// a coroutine awaiting two JavaScript promises
assert.equal(await wasm.sum(later(5, 20), later(5, 22)), 42);

// when_all: both values, or the first error
assert.deepEqual(await wasm.all(later(5, 'a'), later(5, 1)), ['a', 1]);
assert.equal((await rejection(wasm.all(failing(5, 'boom'), later(2000, 0)))).message, 'boom');

// a sender timer, and a timeout for any promise: abortable() completes as soon as
// the signal is aborted, the guarded promise is just ignored afterwards
assert.equal(await wasm.delay(10, 'done'), 'done');
assert.equal(await wasm.abortable(later(5, 'in time'), AbortSignal.timeout(500)), 'in time');
assert.equal((await rejection(wasm.abortable(later(2000, 'too late'), AbortSignal.timeout(20)))).name, 'AbortError');

// an AbortSignal stops the C++ operation: the timer is cancelled for real
const controller = new AbortController();
const aborted = rejection(wasm.delay(60_000, 'never', controller.signal));
setTimeout(() => controller.abort(), 10);
assert.equal((await aborted).name, 'AbortError');
assert.equal((await rejection(wasm.delay(60_000, 'never', AbortSignal.abort()))).name, 'AbortError');

// nothing above waits for the 2 s promises: stopping doesn't need them
const elapsed = Date.now() - start;
assert.ok(elapsed < 1500, `took ${elapsed} ms`);
console.log(`wasm demo: ok (${elapsed} ms)`);
