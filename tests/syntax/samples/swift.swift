// Swift sample file for syntax highlighting comparison
// NOTE: TS grammar has ABI version mismatch (empty query)

import Foundation
import UIKit

// Class definition
class Animal {
    var name: String
    var age: Int
    let species: String

    init(name: String, age: Int, species: String) {
        self.name = name
        self.age = age
        self.species = species
    }

    deinit {
        print("Deallocating \(name)")
    }

    func describe() -> String {
        return "\(name) is a \(species), age \(age)"
    }
}

// Struct definition
struct Point {
    var x: Double
    var y: Double

    mutating func moveBy(dx: Double, dy: Double) {
        x += dx
        y += dy
    }

    static func origin() -> Point {
        return Point(x: 0.0, y: 0.0)
    }
}

// Enum with associated values
enum Result<T> {
    case success(T)
    case failure(Error)

    var isSuccess: Bool {
        switch self {
        case .success:
            return true
        case .failure:
            return false
        }
    }
}

// Protocol definition
protocol Drawable {
    var color: String { get set }
    func draw()
    func resize(factor: Double) -> Self
}

// Protocol extension
extension Drawable {
    func description() -> String {
        return "Drawable with color \(color)"
    }
}

// Class with inheritance and protocol conformance
class Circle: Animal, Drawable {
    var color: String = "red"
    var radius: Double

    init(radius: Double) {
        self.radius = radius
        super.init(name: "circle", age: 0, species: "shape")
    }

    func draw() {
        print("Drawing circle with radius \(radius)")
    }

    func resize(factor: Double) -> Self {
        return self
    }
}

// Guard statement
func processValue(_ value: Int?) {
    guard let unwrapped = value else {
        print("Value is nil")
        return
    }
    print("Value is \(unwrapped)")
}

// Switch with pattern matching
func categorize(_ value: Any) -> String {
    switch value {
    case let x as Int where x > 0:
        return "positive integer"
    case is String:
        return "string"
    case let (x, y) as (Int, Int):
        return "tuple: \(x), \(y)"
    default:
        return "unknown"
    }
}

// Closures
let numbers = [1, 2, 3, 4, 5]
let doubled = numbers.map { $0 * 2 }
let filtered = numbers.filter { value in
    value > 2
}

let sortedNames = ["Charlie", "Alice", "Bob"].sorted {
    $0 < $1
}

// Optionals and chaining
var optionalString: String? = "Hello"
let length = optionalString?.count ?? 0
let forced = optionalString!
let defaulted = optionalString ?? "default"

// Error handling
enum AppError: Error {
    case networkFailure
    case invalidInput(String)
}

func riskyOperation() throws -> String {
    throw AppError.invalidInput("bad data")
}

do {
    let result = try riskyOperation()
    print(result)
} catch AppError.networkFailure {
    print("Network error")
} catch {
    print("Other error: \(error)")
}

// Defer statement
func readFile() {
    let file = open("test.txt")
    defer {
        close(file)
    }
    print("Reading file...")
}

// For-in and while loops
for i in 0..<10 {
    print(i)
}

for name in ["Alice", "Bob"] {
    print("Hello, \(name)")
}

var counter = 10
while counter > 0 {
    counter -= 1
}

repeat {
    counter += 1
} while counter < 5

// Attributes
@available(iOS 13.0, *)
@discardableResult
func modernFunction() -> Bool {
    return true
}

// Static and class members
class MyClass {
    static let shared = MyClass()
    private var data: [String: Any] = [:]
    internal var count: Int = 0
    public var isReady: Bool = false
    fileprivate var secret: String = "hidden"

    subscript(key: String) -> Any? {
        get { return data[key] }
        set { data[key] = newValue }
    }
}

// Typealias and constants
typealias StringDict = [String: String]
let pi: Double = 3.14159
let greeting = "Hello, World!"
let escaped = "tab:\there\nnewline"
let unicode = "\u{1F600}"

// Compiler directives
#if DEBUG
    print("Debug mode")
#elseif TESTING
    print("Test mode")
#else
    print("Release mode")
#endif

// Nil and boolean literals
let nothing: Int? = nil
let yes: Bool = true
let no: Bool = false

// Ternary and range operators
let status = counter > 0 ? "positive" : "zero"
let range = 1...10
let halfOpen = 0..<5
