{-# LANGUAGE GADTs #-}
{-# LANGUAGE TypeFamilies #-}

-- | Module demonstrating Haskell syntax features
module Sample.Haskell
    ( Tree(..)
    , Shape(..)
    , Functor(..)
    , main
    ) where

import Data.List (sort, nub)
import qualified Data.Map as Map
import Data.Maybe hiding (fromJust)

-- Data type with constructors
data Tree a
    = Leaf
    | Node a (Tree a) (Tree a)
    deriving (Show, Eq)

-- Newtype
newtype Wrapper a = Wrapper { unwrap :: a }

-- Record syntax
data Person = Person
    { firstName :: String
    , lastName  :: String
    , age       :: Int
    } deriving (Show)

-- Type alias
type Name = String
type Mapping k v = Map.Map k v

-- Type class definition
class Container f where
    empty  :: f a
    insert :: a -> f a -> f a
    toList :: f a -> [a]

-- Instance
instance Container [] where
    empty  = []
    insert = (:)
    toList = id

-- GADT-style data
data Shape where
    Circle    :: Double -> Shape
    Rectangle :: Double -> Double -> Shape

-- Function with type signature
area :: Shape -> Double
area (Circle r)      = pi * r * r
area (Rectangle w h) = w * h

-- Guards and where clause
classify :: Int -> String
classify n
    | n < 0     = "negative"
    | n == 0    = "zero"
    | n < 10    = "small"
    | otherwise = "large"
  where
    _ = n + 1

-- Pattern matching with case
describe :: Tree a -> String
describe t = case t of
    Leaf       -> "empty"
    Node _ l r -> "node with " ++ show (depth l)
                  ++ " and " ++ show (depth r)

-- Recursive function
depth :: Tree a -> Int
depth Leaf         = 0
depth (Node _ l r) = 1 + max (depth l) (depth r)

-- Do notation and let/in
processIO :: IO ()
processIO = do
    putStrLn "Enter a number:"
    input <- getLine
    let n = read input :: Int
        doubled = n * 2
    if n > 0
        then putStrLn $ "Positive: " ++ show doubled
        else putStrLn "Not positive"

-- Lambda and higher-order functions
transform :: [Int] -> [Int]
transform = map (\x -> x * 2 + 1) . filter (> 0)

-- Infix operators and fixity
infixl 6 |+|
(|+|) :: Int -> Int -> Int
x |+| y = x + y + 1

infixr 5 |:|
(|:|) :: a -> [a] -> [a]
x |:| xs = x : xs

-- Numeric literals
numbers :: (Int, Float, Double)
numbers = (42, 3.14, 1.0e-5)

hexAndOctal :: (Int, Int)
hexAndOctal = (0xFF, 0o77)

-- Char literals
charExamples :: (Char, Char, Char)
charExamples = ('a', '\n', '\\')

-- String with escapes
greeting :: String
greeting = "Hello,\n\tworld!\0"

-- Operators: comparison, logical, arithmetic
ops :: Bool
ops = (10 > 5) && (3 /= 4) || not (True == False)

-- List comprehension with guards
evens :: [Int] -> [Int]
evens xs = [x | x <- xs, x `mod` 2 == 0]

-- Forall (with extension)
identity :: forall a. a -> a
identity x = x

-- Main function with do notation
main :: IO ()
main = do
    let p = Person "John" "Doe" 30
    putStrLn $ firstName p ++ " " ++ lastName p
    print $ area (Circle 5.0)
    print $ classify (-3)
    processIO
