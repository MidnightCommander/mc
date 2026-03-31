// Comment: demonstrate all Go syntax highlighting features
// This file exercises every TS capture from go-highlights.scm

package main

import (
	"fmt"
	"strings"
)

// Keywords
const Pi = 3.14
var counter int

type Shape interface {
	Area() float64
}

type Circle struct {
	Radius float64
}

func (c Circle) Area() float64 {
	return Pi * c.Radius * c.Radius
}

// Map and channel usage
func process() {
	m := map[string]int{"one": 1, "two": 2}
	ch := make(chan int, 10)

	go func() {
		for k, v := range m {
			ch <- v
			_ = k
		}
		close(ch)
	}()

	for val := range ch {
		switch {
		case val == 1:
			continue
		default:
			break
		}
	}
}

// Builtin types
func types() {
	var a int
	var b int8
	var c int16
	var d int32
	var e int64
	var f uint
	var g uint8
	var h uint16
	var i uint32
	var j uint64
	var k uintptr
	var l float32
	var m float64
	var n complex64
	var o complex128
	var p byte
	var q rune
	var r string
	var s bool
	var t error
	_, _, _, _, _, _, _, _, _, _ = a, b, c, d, e, f, g, h, i, j
	_, _, _, _, _, _, _, _, _, _ = k, l, m, n, o, p, q, r, s, t
}

// Builtin functions and constants
func builtins() {
	s := make([]int, 0, 10)
	s = append(s, 1, 2, 3)
	n := len(s)
	c := cap(s)
	_ = n + c

	m := new(Circle)
	_ = m

	x := complex(1.0, 2.0)
	_ = real(x)
	_ = imag(x)

	dst := make([]int, 3)
	copy(dst, s)

	mp := map[string]int{"a": 1}
	delete(mp, "a")

	print("debug")
	println("debug")

	if false {
		panic("error")
	}

	defer func() {
		recover()
	}()
}

// nil, true, false, iota
func constants() {
	var p *int = nil
	b := true
	c := !false
	_ = p
	_ = b
	_ = c
}

// Strings and rune literals
func stringLiterals() {
	s1 := "hello world"
	s2 := `raw string literal
with newlines`
	r1 := 'A'
	_ = s1
	_ = s2
	_ = r1
}

// Labels and goto
func labels() {
	i := 0
loop:
	if i < 10 {
		i++
		goto loop
	}
}

// Channel operator <-
func channelOp() {
	ch := make(chan int)
	go func() { ch <- 42 }()
	val := <-ch
	_ = val
}

// Operators
func operators() {
	a, b := 10, 20
	_ = a + b
	_ = a - b
	_ = a * b
	_ = a / b
	_ = a % b
	_ = a & b
	_ = a | b
	_ = a ^ b
	_ = a << 2
	_ = a >> 2
	_ = a == b
	_ = a != b
	_ = a < b
	_ = a > b
	_ = a <= b
	_ = a >= b
	_ = !false && true || false

	c := 0
	c += 1
	c -= 1
	c *= 2
	c /= 2
	c %= 3
	c &= 0xFF
	c |= 0x01
	c ^= 0x10
	c <<= 1
	c >>= 1

	_ = c
}

// Delimiters: brackets, parens, braces, dots, semicolons, commas
func delimiters() {
	arr := [3]int{1, 2, 3}
	_ = arr[0]
	fmt.Println(arr)
	_ = strings.Contains("abc", "a")
}

/* Block comment
   spanning multiple lines */
