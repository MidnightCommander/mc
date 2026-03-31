// Comment: demonstrate all Java syntax highlighting features
// This file exercises every TS capture from java-highlights.scm

package com.example;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;

// Keywords: class, public, abstract, extends, implements
public abstract class java {

    // Primitive types and void
    byte b = 1;
    short s = 2;
    int i = 3;
    long l = 4L;
    float f = 1.0f;
    double d = 2.0;
    char c = 'A';
    boolean flag = true;

    // true, false, null
    boolean yes = true;
    boolean no = false;
    Object nothing = null;

    // Keywords: abstract, final, static, native, volatile, transient
    static final int MAX = 100;
    volatile int counter = 0;
    transient String temp = "temp";

    // Keywords: synchronized, strictfp
    synchronized void syncMethod() {}

    // Access modifiers: private, protected, public
    private int secret = 0;
    protected int shared = 0;

    // this, super
    void example() {
        this.secret = 1;
        // super.toString();
    }

    // Keywords: if, else, for, while, do, switch, case, default
    void controlFlow() {
        if (flag) {
            int x = 1;
        } else {
            int x = 2;
        }

        for (int j = 0; j < 10; j++) {
            if (j == 5) continue;
            if (j == 8) break;
        }

        while (counter < 10) {
            counter++;
        }

        do {
            counter--;
        } while (counter > 0);

        switch (i) {
            case 1:
                break;
            case 2:
                break;
            default:
                break;
        }
    }

    // Keywords: try, catch, finally, throw, throws
    void exceptions() throws Exception {
        try {
            throw new RuntimeException("error");
        } catch (RuntimeException e) {
            System.err.println(e.getMessage());
        } finally {
            counter = 0;
        }
    }

    // Keywords: new, instanceof, return
    Object create() {
        Object obj = new ArrayList<>();
        if (obj instanceof List) {
            return obj;
        }
        return null;
    }

    // Keywords: enum
    enum Color {
        RED, GREEN, BLUE
    }

    // Keywords: interface, extends
    interface Printable {
        void print();
    }

    // Keywords: record, sealed, permits, non-sealed, yield
    // record Point(int x, int y) {}

    // Annotations
    @Override
    public String toString() {
        return "java instance";
    }

    @SuppressWarnings("unchecked")
    void annotated() {}

    @Deprecated
    void oldMethod() {}

    // Labels
    void labeled() {
        outer:
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                if (j + k > 15) {
                    break outer;
                }
            }
        }
    }

    // Operators
    void operators() {
        int a = 10, b = 20;
        int sum = a + b;
        int diff = a - b;
        int prod = a * b;
        int quot = a / b;
        int rem = a % b;
        boolean eq = a == b;
        boolean ne = a != b;
        boolean lt = a < b;
        boolean gt = a > b;
        boolean le = a <= b;
        boolean ge = a >= b;
        boolean and = true && false;
        boolean or = true || false;
        boolean not = !true;
        int band = a & b;
        int bor = a | b;
        int bxor = a ^ b;
        int bnot = ~a;
        int lsh = a << 2;
        int rsh = a >> 2;
        int ursh = a >>> 2;
        a += 1;
        a -= 1;
        a *= 2;
        a /= 2;
        a %= 3;
        a++;
        a--;
    }

    // Semicolons
    void semi() {
        int x = 1;
        int y = 2;
        int z = 3;
    }

    // Brackets, parens, braces, comma, dot, colon
    void delimiters() {
        int[] arr = {1, 2, 3};
        int val = arr[0];
        Map.Entry<String, Integer> entry = null;
        String result = (val > 0) ? "pos" : "neg";
    }

    // Strings and character literals
    void strings() {
        String s1 = "hello world";
        String s2 = "escape: \n\t\\\"";
        char c1 = 'A';
        char c2 = '\n';
    }

    // Arrow operator (lambda)
    void lambdas() {
        Runnable r = () -> System.out.println("lambda");
        List<Integer> nums = List.of(1, 2, 3);
        nums.forEach(n -> System.out.println(n));
    }
}

/* Block comment
   spanning multiple lines */
