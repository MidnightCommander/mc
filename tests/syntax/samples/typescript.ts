// TypeScript syntax highlighting sample file
// Exercises all TS captures: keyword, comment, string,
// string.special, number.builtin, label, operator, operator.word,
// delimiter, delimiter.special

import { EventEmitter } from "events";

// Interface with generics and readonly
interface Repository<T> {
  readonly id: number;
  name: string;
  findById(id: number): T | undefined;
}

// Type alias with keyof and infer
type KeyOf<T> = keyof T;
type Unwrap<T> = T extends Promise<infer U> ? U : T;
type Assertion = string | number;

// Enum
enum Status {
  Active = 0,
  Inactive = 1,
  Pending = "PENDING",
}

// Abstract class with access modifiers
abstract class BaseEntity {
  private _id: number;
  protected createdAt: Date;
  public static count: number = 0;

  constructor(id: number) {
    this._id = id;
    this.createdAt = new Date();
    BaseEntity.count++;
  }

  abstract validate(): boolean;
  get entityId(): number { return this._id; }
  set entityId(value: number) { this._id = value; }
}

// Class with extends/implements
class UserRepo extends BaseEntity
  implements Repository<User> {
  readonly id: number;
  name: string = "users";
  constructor(id: number) { super(id); this.id = id; }
  validate(): boolean { return this.id > 0; }
  findById(id: number): User | undefined {
    return undefined;
  }
}

// Interface for User
interface User {
  name: string;
  age: number;
  email?: string;
  [key: string]: unknown;
}

// Predefined types and null/undefined
let val: string = "hello";
let num: number = 42;
let flag: boolean = true;
let nothing: null = null;
let missing: undefined = undefined;
let any_val: any = "test";
let unknown_val: unknown = 123;
let voidFn: void = undefined;

// Async/await with arrow functions
const fetchData = async (url: string): Promise<string> => {
  const response = await fetch(url);
  return response as unknown as string;
};

// Generics and type assertions
function identity<T>(arg: T): T {
  return arg;
}
const result = identity<number>(42);
const typed = result as number;

// Template strings
const greeting = `Hello, ${val}!`;
const multi = `Line 1
Line 2: ${num + 1}`;

// Regular expression
const pattern = /^[a-z]+\d*$/gi;

// Operators (arithmetic, comparison, logical, bitwise)
let a = 10, b = 20;
let sum = a + b - a * b / a % b ** 2;
let cmp = a === b || a !== b && a >= b;
let bw = (a & b) | (a ^ b) | ~a;
let sh = (a << 2) >> 2 >>> 1;
let logic = !flag && (a > b || a < b);
a++; b--; a += 5;

// Control flow
if (a > 0) { console.log("positive"); }
else { console.log("non-positive"); }

for (const item of [1, 2, 3]) {
  if (item === 2) break;
  if (item === 0) continue;
}

// Switch, try/catch, keywords
switch (Status.Active) {
  case Status.Active: break;
  default: break;
}
try { throw new Error("oops"); }
catch (e) { console.log(e); }
finally { console.log("done"); }

// Delete, typeof, instanceof, void, in, yield
const obj = { x: 1 }; delete obj.x;
const t = typeof val;
const isDate = new Date() instanceof Date;
void 0;
const hasX = "x" in obj;
function* gen(): Generator<number> { yield 1; yield 2; }

// Declare, namespace, satisfies, override
declare const API_URL: string;
namespace App { export const version = "1.0"; }
const cfg = { port: 80 } satisfies Record<string, number>;
class Child extends BaseEntity {
  override validate(): boolean { return true; }
}

// Numbers and booleans
const int = 42, float = 3.14, hex = 0xff;
const bin = 0b1010, oct = 0o77, big = 1_000_000;
const yes = true, no = false;

// Semicolons, brackets, commas, colons
const arr: number[] = [1, 2, 3];
const map: { [k: string]: number } = { a: 1 };

/* Block comment with multiple lines
   spanning across several lines */
