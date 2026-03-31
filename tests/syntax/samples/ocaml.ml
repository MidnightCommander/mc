(* OCaml syntax sample file *)

(* Basic let bindings *)
let x = 42
let pi = 3.14159
let name = "OCaml"
let ch = 'A'
let flag = true
let nothing = false

(* Let with type annotation *)
let greeting : string = "Hello, world!"

(* Function definitions *)
let add a b = a + b
let multiply x y = x * y

let rec factorial n =
  if n <= 0 then 1
  else n * factorial (n - 1)

let rec fibonacci = function
  | 0 -> 0
  | 1 -> 1
  | n -> fibonacci (n - 1) + fibonacci (n - 2)

(* Pattern matching *)
let describe_number n =
  match n with
  | 0 -> "zero"
  | 1 -> "one"
  | n when n < 0 -> "negative"
  | _ -> "other"

(* Variant types *)
type color =
  | Red
  | Green
  | Blue
  | Custom of int * int * int

type 'a option_like =
  | None_like
  | Some_like of 'a

(* Record types *)
type person = {
  name : string;
  mutable age : int;
  email : string;
}

(* Using records *)
let john = { name = "John"; age = 30; email = "j@x.com" }
let _ = john.name
let () = john.age <- 31

(* Tuple and list *)
let pair = (1, "hello")
let nums = [1; 2; 3; 4; 5]
let combined = 0 :: nums

(* Higher-order functions *)
let double_all = List.map (fun x -> x * 2)
let positives = List.filter (fun x -> x > 0)

(* Let-in expressions *)
let result =
  let a = 10 in
  let b = 20 in
  a + b

(* If-then-else *)
let abs_val x =
  if x >= 0 then x
  else -x

(* For and while loops *)
let print_range () =
  for i = 1 to 10 do
    Printf.printf "%d " i
  done;
  let j = ref 10 in
  while !j > 0 do
    j := !j - 1
  done

(* Try-with exception handling *)
exception Not_found_custom of string
exception Empty_error

let safe_divide a b =
  try
    if b = 0 then raise (Not_found_custom "div/0")
    else a / b
  with
  | Not_found_custom msg ->
    Printf.printf "Error: %s\n" msg; 0
  | Empty_error -> 0

(* Module definition *)
module IntSet = struct
  type t = int list
  let empty = []
  let add x s = x :: s
  let mem x s = List.mem x s
  let to_list s = s
end

(* Module signature *)
module type STACK = sig
  type 'a t
  val empty : 'a t
  val push : 'a -> 'a t -> 'a t
  val pop : 'a t -> 'a t
  val top : 'a t -> 'a
  exception Stack_empty
end

(* Functor *)
module MakeStack (E : sig type t end) = struct
  type elt = E.t
  type t = elt list
  exception Stack_empty
  let empty = []
  let push x s = x :: s
  let pop = function
    | [] -> raise Stack_empty
    | _ :: t -> t
  let top = function
    | [] -> raise Stack_empty
    | h :: _ -> h
end

(* Open and include *)
open Printf

(* Class definition *)
class point x_init y_init = object
  val mutable x = x_init
  val mutable y = y_init
  method get_x = x
  method get_y = y
  method move dx dy =
    x <- x + dx;
    y <- y + dy
end

(* Labeled arguments *)
let create ~name ~age = { name; age; email = "" }

(* Operators *)
let _ = 1 + 2 - 3 * 4
let _ = 1.0 = 2.0
let _ = "a" < "b"
let _ = 1 > 0
let _ = [1] :: [[2]]
let _ = x := 5
let _ = !x
let _ = 1 |> succ |> succ

(* Assert and lazy *)
let _ = assert (1 + 1 = 2)
let lazy_val = lazy (factorial 10)

(* Begin-end block *)
let do_stuff () =
  begin
    print_string "hello";
    print_newline ()
  end

(* External declaration *)
external c_func : int -> int = "c_func_impl"

(* Polymorphic variant *)
let use_poly = function
  | `Red -> "red"
  | `Blue -> "blue"
  | _ -> "other"

(* Semicolons and double semicolons *)
let () = print_endline "done";;
let () = print_endline "final"
