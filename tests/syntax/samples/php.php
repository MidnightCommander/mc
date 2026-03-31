<?php
// Sample file demonstrating PHP syntax highlighting features.
// Exercises all captures from the TS query override.

// Keywords -> brightmagenta
namespace App\Models;

use App\Traits\HasFactory;
require 'config.php';
require_once 'helpers.php';
include 'utils.php';
include_once 'constants.php';

// Class definition
abstract class Animal implements Serializable
{
    use HasFactory;

    const MAX_AGE = 100;
    public readonly string $name;
    protected int $age;
    private bool $alive = true;
    static $count = 0;

    public function __construct(string $name, int $age)
    {
        $this->name = $name;
        $this->age = $age;
        self::$count++;
    }

    abstract public function speak(): string;

    final public function getName(): string
    {
        return $this->name;
    }
}

class Dog extends Animal
{
    public function speak(): string
    {
        return "Woof!";
    }
}

interface Moveable
{
    public function move(): void;
}

trait Runnable
{
    public function run(): void
    {
        echo "Running\n";
    }
}

// Control flow
if (true) {
    echo "yes";
} elseif (false) {
    echo "maybe";
} else {
    echo "no";
}

// Switch
switch ($value) {
    case 1:
        break;
    case 2:
        continue;
    default:
        break;
}

// Loops
for ($i = 0; $i < 10; $i++) {
    if ($i == 5) break;
}

foreach ($items as $key => $val) {
    echo "$key: $val\n";
}

while (true) {
    break;
}

do {
    break;
} while (false);

// Alternative syntax
for ($i = 0; $i < 5; $i++):
endfor;

foreach ($items as $item):
endforeach;

if (true):
endif;

switch ($x):
endswitch;

while (false):
endwhile;

declare(strict_types=1);
// enddeclare would go here

// Match expression (PHP 8)
$result = match($x) {
    1 => "one",
    2 => "two",
    default => "other",
};

// Exception handling
try {
    throw new \Exception("error");
} catch (\Exception $e) {
    echo $e->getMessage();
} finally {
    echo "cleanup";
}

// Constants -> brightmagenta
$a = null;
$b = true;
$c = false;

// UPPERCASE constants -> white
define('API_KEY', 'secret');
$version = PHP_VERSION;

// Operators -> white
$sum = 1 + 2;
$diff = 5 - 3;
$prod = 4 * 2;
$quot = 10 / 2;
$mod = 7 % 3;
$pow = 2 ** 8;
$concat = "a" . "b";
$concat_assign = "a";
$concat_assign .= "b";

// Comparison operators
$eq = (1 == 2);
$seq = (1 === 2);
$ne = (1 != 2);
$sne = (1 !== 2);
$ne2 = (1 <> 2);
$lt = (1 < 2);
$gt = (1 > 2);
$lte = (1 <= 2);
$gte = (1 >= 2);
$cmp = (1 <=> 2);

// Logical operators
$and = true && false;
$or = true || false;
$not = !true;
$band = 1 & 2;
$bor = 1 | 2;
$bxor = 1 ^ 2;
$bnot = ~0;
$shl = 1 << 4;
$shr = 16 >> 2;

// Assignment
$x = 1;
$x += 1;
$x -= 1;
$x *= 2;
$x /= 2;

// Null coalescing
$val = $x ?? "default";

// Arrow and scope
$dog = new Dog("Rex", 5);
$dog->speak();
Dog::$count;
echo Animal::MAX_AGE;

// Function definitions and calls -> yellow
function add(int $a, int $b): int
{
    return $a + $b;
}

$result = add(1, 2);
$len = strlen("hello");
$arr = array_map(fn($x) => $x * 2, [1, 2, 3]);

// Arrow function
$double = fn($n) => $n * 2;

// Variables -> brightgreen
$simple = "value";
$arr = [1, 2, 3];

// Strings
$double = "Hello, $name!\n";
$single = 'no interpolation';
$heredoc = <<<EOT
Heredoc string
with $simple interpolation
EOT;

$nowdoc = <<<'EOT'
Nowdoc string
no interpolation here
EOT;

// Brackets -> brightcyan
$nested = [[1, 2], ["a" => 1]];

// Semicolons -> brightmagenta
echo "done";

// Labels
goto end;
end:
echo "end\n";

// Global and unset
global $globalVar;
unset($globalVar);

// List, print, exit
list($first, $second) = [1, 2];
print "output\n";
// exit(0);

// Logical keywords
$r1 = true and false;
$r2 = true or false;
$r3 = true xor false;

// yield
function gen()
{
    yield 1;
    yield 2;
}

// Clone and instanceof
$copy = clone $dog;
$check = $dog instanceof Animal;

/* Block comment */
# Hash comment
// Line comment
