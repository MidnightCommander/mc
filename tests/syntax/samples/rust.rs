// Comment: demonstrate all Rust syntax highlighting features
// This file exercises every TS capture from rust-highlights.scm

use std::collections::HashMap;
use std::fmt;

// Keywords: struct, impl, pub, fn, let, mut, return, if, else
pub struct Point {
    pub x: f64,
    pub y: f64,
}

impl Point {
    pub fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }

    pub fn distance(&self, other: &Point) -> f64 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        (dx * dx + dy * dy).sqrt()
    }
}

// Trait
pub trait Drawable {
    fn draw(&self);
}

impl Drawable for Point {
    fn draw(&self) {
        println!("Drawing at ({}, {})", self.x, self.y);
    }
}

impl fmt::Display for Point {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "({}, {})", self.x, self.y)
    }
}

// Enum with variants
pub enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
}

// Match expression
fn area(s: &Shape) -> f64 {
    match s {
        Shape::Circle(r) => std::f64::consts::PI * r * r,
        Shape::Rectangle(w, h) => w * h,
    }
}

// Keywords: for, in, while, loop, break, continue
fn loops() {
    for i in 0..10 {
        if i == 5 {
            continue;
        }
        if i == 8 {
            break;
        }
    }

    let mut n = 0;
    while n < 10 {
        n += 1;
    }

    loop {
        break;
    }
}

// Async/await
async fn fetch_data() -> Result<String, Box<dyn std::error::Error>> {
    let data = async { String::from("hello") }.await;
    Ok(data)
}

// Keywords: const, static, type, where, extern, unsafe, mod, move
const MAX_SIZE: usize = 100;
static GLOBAL: &str = "global";

type Pair<T> = (T, T);

mod inner {
    pub fn helper() {}
}

extern "C" {
    fn abs(input: i32) -> i32;
}

fn use_move() {
    let data = vec![1, 2, 3];
    let closure = move || {
        println!("{:?}", data);
    };
    closure();
}

fn generic<T>(val: T) -> T
where
    T: Clone,
{
    val.clone()
}

// Keywords: ref, as, dyn, yield
fn references() {
    let x = 42;
    let ref r = x;
    let _ = r as &i32;
    let _: Box<dyn Drawable> = Box::new(Point::new(0.0, 0.0));
}

// self, crate, super
fn module_refs() {
    let _ = crate::inner::helper;
    // super::something() would refer to parent module
}

// Boolean literals: true, false
fn booleans() -> bool {
    let a = true;
    let b = false;
    a && !b
}

// Macros
fn macros() {
    println!("formatted: {}", 42);
    eprintln!("error message");
    format!("template {}", "value");
    vec![1, 2, 3];
    assert!(true);
    assert_eq!(1, 1);
    dbg!(42);
    todo!("not yet");
}

macro_rules! my_macro {
    ($x:expr) => {
        $x + 1
    };
}

// Types: type_identifier and primitive_type
fn type_examples() {
    let _a: bool = true;
    let _b: char = 'z';
    let _c: i8 = -1;
    let _d: i16 = -2;
    let _e: i32 = -3;
    let _f: i64 = -4;
    let _g: u8 = 1;
    let _h: u16 = 2;
    let _i: u32 = 3;
    let _j: u64 = 4;
    let _k: f32 = 1.0;
    let _l: f64 = 2.0;
    let _m: isize = -5;
    let _n: usize = 5;
    let _o: str = *"hello";
    let _p: String = String::from("world");
    let _q: Vec<i32> = Vec::new();
    let _r: Option<i32> = Some(42);
    let _s: Result<i32, String> = Ok(0);
    let _t: HashMap<String, i32> = HashMap::new();
}

// Some/None/Ok/Err
fn option_result() {
    let x: Option<i32> = Some(10);
    let y: Option<i32> = None;
    let a: Result<i32, &str> = Ok(1);
    let b: Result<i32, &str> = Err("fail");
    _ = (x, y, a, b);
}

// Number literals
fn numbers() {
    let _dec = 42;
    let _hex = 0xFF;
    let _oct = 0o77;
    let _bin = 0b1010;
    let _flt = 3.14;
    let _sci = 1e10;
    let _sep = 1_000_000;
}

// Strings
fn strings() {
    let _s1 = "hello world";
    let _s2 = r#"raw string"#;
    let _c1 = 'A';
    let _c2 = '\n';
}

// Attributes
#[derive(Debug, Clone)]
struct Tagged {
    value: i32,
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}

// Labels
fn labeled_loop() {
    'outer: for i in 0..10 {
        for j in 0..10 {
            if i + j > 15 {
                break 'outer;
            }
        }
    }
}

/* Block comment
   spanning multiple lines */
