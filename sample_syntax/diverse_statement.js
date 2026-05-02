#!/usr/bin/env node

// Strict mode (optional, but enables private fields, etc.)
'use strict';

import { setTimeout } from 'os';

// ======================= DECLARATIONS =======================

// Variables: var, let, const
var globalVar = 42;
let blockScoped = 'hello';
const constant = 3.14;

// Primitive types
let undef = undefined;
let nullVal = null;
let bool = true;
let number = 123.45;
let bigInt = 9007199254740991n;
let symbol = Symbol('unique');
let string = "double";
let template = `interpolated ${number}`;

// Objects, arrays, functions
let obj = { a: 1, b: 2 };
let arr = [1, 2, 3];
let funcExpr = function() { return 'func'; };

// Regular expression
let regex = /^[a-z]+$/gi;

// Built-in objects
let date = new Date();
let map = new Map([[1, 'one'], [2, 'two']]);
let set = new Set([1, 2, 3]);
let weakMap = new WeakMap();
let weakSet = new WeakSet();
let arrayBuffer = new ArrayBuffer(16);
let int32Array = new Int32Array(arrayBuffer);

// ======================= OPERATORS =======================

// Arithmetic
let sum = 1 + 2;
let diff = 5 - 3;
let prod = 2 * 3;
let quot = 10 / 2;
let mod = 10 % 3;
let exp = 2 ** 3;

// Assignment
let x = 10;
x += 5;
x -= 2;
x *= 3;
x /= 2;
x %= 4;
x **= 2;
x &&= 2;          // logical AND assignment
x ||= 0;          // logical OR assignment
x ??= 100;        // nullish coalescing assignment

// Comparison
let eq = (1 == '1');
let strictEq = (1 === '1');
let neq = (1 != '2');
let strictNeq = (1 !== '2');
let lt = 2 < 3;
let gt = 5 > 4;
let le = 3 <= 3;
let ge = 4 >= 4;

// Logical
let and = true && false;
let or = true || false;
let not = !true;
let nullish = null ?? 'default';
let optionalChain = obj?.a?.b?.c;   // optional chaining

// Bitwise
let bitAnd = 5 & 3;
let bitOr = 5 | 3;
let bitXor = 5 ^ 3;
let bitNot = ~5;
let leftShift = 5 << 1;
let rightShift = 5 >> 1;
let zeroFillRightShift = 5 >>> 1;

// typeof, instanceof, in, delete, new, void
let type = typeof 123;
let isInstance = arr instanceof Array;
let hasProp = 'a' in obj;
delete obj.a;
let newObj = new Object();
let voidResult = void 0;

// ======================= CONTROL FLOW =======================

// if / else
if (true) {
    // do something
} else if (false) {
    // ...
} else {
    // ...
}

// switch / case
switch (x) {
    case 1:
        break;
    case 2:
    case 3:
        break;
    default:
        break;
}

// for loops
for (let i = 0; i < 10; i++) { }
for (let key in obj) { }
for (let value of arr) { }

const asyncIterable = {
    [Symbol.asyncIterator]: async function* () {
        yield 1;
        await new Promise(r => setTimeout(r, 10));
        yield 2;
    }
};
(async () => {
    for await (const val of asyncIterable) {
        console.log(val);
    }
})();
for await (let value of asyncIterable) { } 

// while / do-while
while (false) { }
do { } while (false);

// break / continue with labels
outer: for (let i = 0; i < 5; i++) {
    inner: for (let j = 0; j < 5; j++) {
        if (j === 2) continue inner;
        if (i === 3) break outer;
    }
}

// ======================= FUNCTIONS =======================

// Function declaration
function regularFunc(a, b = 10, ...rest) {
    return a + b + rest.length;
}

// Function expression
const funcExpr2 = function(a, b) { return a * b; };

// Arrow function
const arrow = (x) => x * 2;
const implicitReturn = x => x * 2;
const multiLineArrow = (x, y) => {
    let sum = x + y;
    return sum;
};

// Generator function
function* generator() {
    yield 1;
    yield* [2, 3];
    return 4;
}

// Async function
async function asyncFunc() {
    return await Promise.resolve(42);
}

// Async generator
async function* asyncGenerator() {
    yield await Promise.resolve(1);
    yield* [2, 3];
}

// Methods in objects
const objWithMethods = {
    method() { return 'method'; },
    get getter() { return this._value; },
    set setter(val) { this._value = val; },
    async asyncMethod() { return 'async'; },
    *generatorMethod() { yield 42; }
};

// Default, rest, destructuring parameters
function complexParams({ a, b = 2 } = {}, [c, d] = [3, 4], ...rest) {
    return { a, b, c, d, rest };
}

// ======================= CLASSES =======================

class Base {
    #privateField = 42;            // private field
    static staticField = 'static';
    static #privateStatic = 'secret';
    
    constructor(publicField) {
        this.publicField = publicField;
    }
    
    publicMethod() {
        return this.#privateField;
    }
    
    #privateMethod() {
        return 'private';
    }
    
    static staticMethod() {
        return Base.staticField;
    }
    
    static {
        // static initialization block
        console.log('static block executed');
    }
}

class Derived extends Base {
    constructor(publicField, extra) {
        super(publicField);
        this.extra = extra;
    }
    
    override() {
        return super.publicMethod();
    }
}

// Class expression
const ClassExpr = class { };

// ======================= MODULES (top‑level) =======================

// Assuming this file is treated as a module, these are valid.
export const exportedVar = 123;
export function exportedFunc() { }
export default class DefaultExport { }
export { Base as AliasBase };
// Dynamic import (returns a promise)
const dynamicImport = import('./some-module.js').catch(console.log);

// import.meta
console.log(import.meta.url);

// ======================= DESTRUCTURING & SPREAD/REST =======================

// Array destructuring
let [first, second, ...restArr] = [1, 2, 3, 4];
let [, , third] = [1, 2, 3];

// Object destructuring
let { a: aAlias, b: bAlias = 100, ...restObj } = { a: 1, b: 2, c: 3 };

// Spread in arrays/objects
let combinedArr = [...arr, 4, 5];
let combinedObj = { ...obj, newProp: 'value' };

// Rest parameters (already shown in functions)
function restDemo(...args) { return args; }

// ======================= TEMPLATE LITERALS =======================

let name = 'World';
let greeting = `Hello, ${name}!`;
let tagged = (strings, ...values) => strings.reduce((acc, s, i) => acc + s + (values[i] || ''), '');
let result = tagged`Hello ${name}, your number is ${42}`;

// ======================= PROMISES & ASYNC/AWAIT =======================

let promise = new Promise((resolve, reject) => {
    setTimeout(() => resolve('done'), 100);
});
promise
    .then(val => console.log(val))
    .catch(err => console.log(err))
    .finally(() => console.log('finally'));

Promise.all([Promise.resolve(1), Promise.resolve(2)])
    .then(([one, two]) => console.log(one, two));

Promise.race([promise, new Promise((_, reject) => setTimeout(reject, 50))])
    .catch(err => console.log('race lost'));

async function runAsync() {
    try {
        const val = await promise;
        console.log(val);
    } catch (err) {
        console.log(err);
    }
}
runAsync();

// ======================= ERROR HANDLING =======================

try {
    throw new Error('Something went wrong');
} catch (err) {
    console.log(err.message);
} finally {
    console.log('cleanup');
}

// Custom error
class CustomError extends Error {
    constructor(message) {
        super(message);
        this.name = 'CustomError';
    }
}
try {
    throw new CustomError('oops');
} catch (e) {
    if (e instanceof CustomError) {
        // handle
    }
}

// ======================= ITERATORS & ITERABLES =======================

let iterable = {
    [Symbol.iterator]: function* () {
        yield 1;
        yield 2;
        yield 3;
    }
};
for (let val of iterable) {
    console.log(val);
}

// ======================= SYMBOLS =======================

let sym = Symbol('description');
let globalSym = Symbol.for('global');
let symKey = Symbol.keyFor(globalSym);
let wellKnown = Symbol.iterator;

// ======================= PROXY & REFLECT =======================

let target = { message: 'hello' };
let handler = {
    get: function(obj, prop) {
        return prop in obj ? obj[prop] : 'default';
    },
    set: function(obj, prop, value) {
        if (prop === 'age' && typeof value !== 'number') {
            throw new TypeError('age must be a number');
        }
        obj[prop] = value;
        return true;
    }
};
let proxy = new Proxy(target, handler);
proxy.age = 30;
console.log(proxy.nonExistent); // 'default'

Reflect.set(target, 'newProp', 123);
let hasPropReflect = Reflect.has(target, 'newProp');

// ======================= BIGINT =======================

let big = 12345678901234567890n;
let bigSum = big + 1n;
let bigCompare = big > 100n;

// ======================= OPTIONAL CHAINING & NULLISH COALESCING =======================

let nested = { foo: { bar: null } };
let value = nested?.foo?.bar ?? 'default value';

// ======================= LOGICAL ASSIGNMENT =======================

let a = null;
a ??= 'assigned';   // a becomes 'assigned'
let b = 5;
b &&= 10;           // b becomes 10 (5 && 10)
let c = 0;
c ||= 100;          // c becomes 100 (0 || 100)

// ======================= EVAL & ARGUMENTS =======================

let evaled = eval('2 + 2'); // 4
function argumentsDemo() {
    console.log(arguments[0]);
}
argumentsDemo('test');

// ======================= DEBUGGER =======================

debugger; // stops execution if debugger attached

// ======================= GLOBAL OBJECT =======================

console.log(globalThis); // global object in any environment

// ======================= NEW.TARGET & IMPORT.META =======================

function Constructor() {
    if (!new.target) throw new Error('must be called with new');
}
new Constructor();

// import.meta already used above

// ======================= TYPED ARRAYS & ARRAYBUFFER =======================

let buffer = new ArrayBuffer(8);
let view = new DataView(buffer);
view.setInt32(0, 42);
let int32 = view.getInt32(0);

let uint8 = new Uint8Array([1, 2, 3]);

// ======================= MAP, SET, WEAKMAP, WEAKSET =======================

map.set('key', 'value');
set.add(4);
weakMap.set({}, 'value');
weakSet.add({});

// ======================= JSON =======================

let jsonString = JSON.stringify({ a: 1, b: 2 });
let parsed = JSON.parse(jsonString);

// ======================= DATE & MATH =======================

let now = Date.now();
let mathPow = Math.pow(2, 3);
let random = Math.random();

// ======================= REGEXP EXTENDED FEATURES =======================

let namedGroups = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/;
let match = namedGroups.exec('2025-03-30');
if (match) console.log(match.groups.year);

// ======================= TOP-LEVEL AWAIT (in modules) =======================
// If this file is a module, we can use top‑level await
// const topLevelAwait = await Promise.resolve('top-level');

// ======================= FINAL EXECUTION =======================

// An IIFE to demonstrate everything is valid
(function main() {
    console.log('Program executed successfully');
    // Use some of the defined variables to avoid "unused" warnings (if any)
    console.log(globalVar, blockScoped, constant, undef, nullVal, bool, number, bigInt, symbol, string, template);
    console.log(obj, arr, funcExpr, regex, date, map, set, weakMap, weakSet, arrayBuffer, int32Array);
    console.log(regularFunc(1), arrow(5), generator().next().value);
    console.log(new Derived('hello', 'world'));
})();

// Export something (if module)
export { regularFunc, arrow };
