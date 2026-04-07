// Sample JavaScript file demonstrating syntax highlighting features
// Exercises all tree-sitter captures from javascript-highlights.scm

/* Multi-line
   comment block */

// Keywords
var x = 1;
let y = 2;
const z = 3;

if (x > 0) {
    return x;
} else {
    throw new Error("negative");
}

for (let i = 0; i < 10; i++) {
    if (i === 5) break;
    if (i === 3) continue;
}

while (true) {
    break;
}

do {
    x++;
} while (x < 100);

switch (x) {
    case 1:
        break;
    default:
        break;
}

try {
    delete x.prop;
} catch (e) {
    debugger;
} finally {
    void 0;
}

// Classes and inheritance
class Animal extends Object {
    static count = 0;
    constructor(name) {
        super();
        this.name = name;
    }
    get info() { return this.name; }
    set info(val) { this.name = val; }
}

// Import/export
import { foo } from "module";
export const bar = 42;

// Functions and async/await
function greet(name) {
    return "Hello " + name;
}

async function fetchData() {
    const result = await fetch("/api");
    return result;
}

// Arrow functions and spread
const add = (a, b) => a + b;
const arr = [1, ...other];

// typeof, instanceof, in, of
typeof x instanceof Object;
for (const key in obj) {}
for (const val of arr) {}

// yield and with
function* gen() {
    yield 1;
    yield 2;
}

with (obj) {
    x = 1;
}

// Literals: true, false, null, undefined
const a = true;
const b = false;
const c = null;
const d = undefined;

// Numbers
const num1 = 42;
const num2 = 3.14;
const num3 = 0xff;
const num4 = 1e10;

// Strings and template literals
const str1 = "double quoted";
const str2 = 'single quoted';
const tmpl = `template ${x} literal`;

// Regular expression
const re = /pattern/gi;

// Operators
x = y + z - x * y / z % 2;
x += 1; x -= 1; x *= 2; x /= 2; x %= 3;
x **= 2; x &&= true; x ||= false;
let cmp = x === y || x !== y && x <= z;
let bit = x & y | z ^ ~x;
x << 2; x >> 1; x >>> 0;
x++; x--;

// Semicolons, brackets, colons, commas
const obj = { a: 1, b: [2, 3] };
(function() {})();

// Labels
outer: for (let i = 0; i < 5; i++) {
    inner: for (let j = 0; j < 5; j++) {
        if (j === 2) break outer;
    }
}

// as keyword
const val = x as any;
