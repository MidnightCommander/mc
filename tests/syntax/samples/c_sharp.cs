// C# syntax sample: demonstrates all TS capture groups
// This file exercises every capture in c_sharp-highlights.scm

using System;
using System.Collections.Generic;
using System.Linq;

namespace SyntaxDemo
{
    // Type declaration keywords (@keyword.other -> white)
    public enum Color { Red, Green, Blue }

    public delegate void Handler(string msg);

    public interface IShape
    {
        double Area();
    }

    public struct Point
    {
        public int X;
        public int Y;
    }

    public record Person(string Name, int Age);

    // Access modifiers (@function.special -> brightred)
    // public, private, internal
    internal class Helper { }

    // Main class
    public class Demo : IShape
    {
        // Keywords (@keyword -> yellow)
        private readonly int _value;
        protected virtual int Value => _value;
        public static int Counter = 0;
        const int MaxSize = 100;
        volatile bool _running;

        public Demo(int value)
        {
            this._value = value;
        }

        // Operators (@operator.word -> yellow)
        public int Arithmetic(int a, int b)
        {
            int r;
            r = a + b;
            r = a - b;
            r = a * b;
            r = a / b;
            r = a % b;
            r++;
            r--;
            r += a;
            r -= b;
            r *= 2;
            r /= 2;
            r %= 3;
            return r;
        }

        // Comparison and logical operators
        public bool Compare(int a, int b)
        {
            if (a == b) return true;
            if (a != b) return false;
            if (a < b && a > 0) return true;
            if (a >= b || a <= 0) return false;
            if (!true) return false;
            return a > b;
        }

        // Bitwise operators
        public int Bitwise(int a, int b)
        {
            int r;
            r = a & b;
            r = a | b;
            r = a ^ b;
            r = ~a;
            r = a << 2;
            r = a >> 1;
            return r;
        }

        // Null coalescing and lambda
        public string NullOps(string s)
        {
            string result = s ?? "default";
            Func<int, int> square = x => x * x;
            return result;
        }

        // Strings (@string -> green)
        public void Strings()
        {
            string s1 = "Hello, World!";
            string s2 = @"Verbatim\nstring";
            string s3 = $"Interpolated {_value}";
        }

        // Character literals (@string.special -> brightgreen)
        public void Chars()
        {
            char c1 = 'A';
            char c2 = '\n';
            char c3 = '\\';
        }

        // null, true, false (@keyword -> yellow)
        public void Literals()
        {
            object obj = null;
            bool t = true;
            bool f = false;
        }

        // Predefined types (@type -> yellow)
        public void Types()
        {
            int i = 0;
            long l = 0L;
            float fl = 1.0f;
            double d = 1.0;
            bool b = true;
            string s = "";
            char c = 'x';
            byte by = 0;
            decimal dec = 1.0m;
            object o = null;
        }

        // Labels (@label -> cyan)
        public void LabelDemo()
        {
            goto done;
        done:
            return;
        }

        // Control flow keywords
        public double Area()
        {
            return 0.0;
        }

        // switch, try/catch/finally, for/foreach, while/do
        public void ControlFlow(int x)
        {
            switch (x)
            {
                case 0:
                    break;
                default:
                    break;
            }

            try
            {
                throw new Exception("error");
            }
            catch (Exception)
            {
                // handled
            }
            finally
            {
                Counter++;
            }

            for (int i = 0; i < 10; i++) { }
            foreach (var item in new int[] { 1, 2, 3 }) { }
            while (x > 0) { x--; }
            do { x++; } while (x < 5);
        }

        // More keywords: as, is, typeof, sizeof, checked
        public void MoreKeywords()
        {
            object o = "test";
            if (o is string) { }
            string s = o as string;
            Type t = typeof(int);
            int sz = sizeof(int);
            checked { int big = int.MaxValue; }
            unchecked { int over = int.MaxValue + 1; }
        }

        // async/await, yield, var, ref, out, params
        public async void AsyncDemo()
        {
            await System.Threading.Tasks.Task.Delay(1);
        }

        // Delimiters (@delimiter -> brightcyan)
        // Covered by: . , : ( ) [ ] { }

        // Semicolons (@delimiter.special -> brightmagenta)
        // Every statement-ending ;

        // unsafe, fixed, stackalloc, lock, goto
        public unsafe void UnsafeDemo()
        {
            int val = 42;
            int* p = &val;
            fixed (char* cp = "hello") { }
            lock (this) { }
        }

        // sealed, abstract, explicit, implicit, partial
        // virtual, override, where, volatile
    }

    // sealed class
    public sealed class Final : Demo
    {
        public Final() : base(0) { }
        public override int Value => 0;
    }

    public abstract class AbstractBase
    {
        public abstract void DoWork();
    }
}
