// Comment: demonstrate all Kotlin syntax highlighting features
// This file exercises every TS capture from kotlin-highlights.scm

package com.example

import java.util.Collections

// Hard keywords: class, fun, if, else, for, while, when, return, val, var
class Animal(val name: String, var age: Int) {

    fun greet(): String {
        return "Hello, $name"
    }

    fun describe(): String {
        if (age > 10) {
            return "Old $name"
        } else {
            return "Young $name"
        }
    }
}

// Hard keywords: object, interface, is, as, in, this, super, throw, try
object Singleton {
    val instance = "singleton"
}

interface Drawable {
    fun draw()
}

open class Shape : Drawable {
    override fun draw() {
        println("Drawing shape")
    }
}

class Circle(val radius: Double) : Shape() {
    override fun draw() {
        super.draw()
        println("Drawing circle r=$radius")
    }

    fun check(obj: Any) {
        if (obj is String) {
            println(obj as String)
        }
        if (obj !is Int) {
            throw IllegalArgumentException("not int")
        }
    }
}

// Hard keywords: do, while, for, in, when, typealias
typealias StringList = List<String>

fun loops() {
    for (i in 1..10) {
        if (i == 5) continue
        if (i == 8) break
    }

    var n = 0
    while (n < 10) {
        n++
    }

    do {
        n--
    } while (n > 0)

    when (n) {
        0 -> println("zero")
        1 -> println("one")
        else -> println("other")
    }
}

// Soft keywords: by, catch, constructor, finally, get, init, set, where
class Config {
    var value: Int = 0
        get() = field
        set(v) { field = v }

    constructor(initial: Int) : this() {
        value = initial
    }

    init {
        println("Config created")
    }
}

fun <T> constrained(item: T) where T : Comparable<T> {
    println(item)
}

fun tryCatch() {
    try {
        val x = 1 / 0
    } catch (e: ArithmeticException) {
        println("caught: ${e.message}")
    } finally {
        println("done")
    }
}

// package and import -> brown (already at top)

// Modifier keywords
abstract class Base {
    abstract fun compute(): Int
    open fun info() = "base"
    protected fun helper() {}
    internal fun restricted() {}
}

data class Point(val x: Double, val y: Double)
enum class Direction { NORTH, SOUTH, EAST, WEST }
sealed class Result
annotation class MyAnnotation
companion object {}

inline fun <reified T> check(value: Any): Boolean = value is T

class Outer {
    inner class Inner {
        fun access() = "inner"
    }
}

const val PI = 3.14159
private val secret = "hidden"
public val visible = "shown"
final class Immutable

suspend fun coroutine() {}
tailrec fun factorial(n: Int, acc: Int = 1): Int =
    if (n <= 1) acc else factorial(n - 1, n * acc)

infix fun Int.times(str: String) = str.repeat(this)
operator fun Point.plus(other: Point) = Point(x + other.x, y + other.y)

fun varargExample(vararg items: Int) = items.sum()
fun crossinlineExample(crossinline block: () -> Unit) {}
fun noinlineExample(noinline block: () -> Unit) {}

lateinit var lateValue: String
val lazyVal by lazy { "computed" }

// Comments
/* Block comment
   spanning multiple lines */

// Strings
fun strings() {
    val s1 = "hello world"
    val s2 = """
        multi
        line
        string
    """.trimIndent()
    val ch = 'A'
}

// Annotations
@MyAnnotation
fun annotated() {}

@Deprecated("use newMethod")
fun oldMethod() {}

// Built-in types
fun types() {
    val a: Double = 1.0
    val b: Float = 2.0f
    val c: Long = 3L
    val d: Int = 4
    val e: Short = 5
    val f: Byte = 6
    val g: Char = 'Z'
    val h: Boolean = true
    val i: Array<Int> = arrayOf(1, 2, 3)
    val j: String = "text"
    val k: ByteArray = byteArrayOf(1, 2, 3)
}

// Labels
fun labeled() {
    loop@ for (i in 1..10) {
        for (j in 1..10) {
            if (i + j > 15) break@loop
        }
    }
}

// Operators
fun operators() {
    var a = 10
    val b = 20
    val sum = a + b
    val diff = a - b
    val prod = a * b
    val quot = a / b
    val rem = a % b
    val eq = a == b
    val ne = a != b
    val lt = a < b
    val gt = a > b
    val le = a <= b
    val ge = a >= b
    val and = true && false
    val or = true || false
    val not = !true
    a += 1
    a -= 1
    a *= 2
    a /= 2
    a %= 3
    a++
    a--

    // Special operators
    val range = 1..10
    val elvis = null ?: "default"
    val ref = String::class
    val force = nullable!!

    // Arrow and safe cast
    val fn: (Int) -> Int = { it * 2 }
    val safe = obj as? String
}

// Delimiters: dot, comma, colon, semicolons
fun delimiters() {
    val x = listOf(1, 2, 3)
    val y: Int = x.size
    val z = 1; val w = 2
}
