# R language sample file exercising all TS captures

# Library imports
library(ggplot2)
library(dplyr)
require(stats)

# Variable assignment with different operators
x <- 10
y <<- 20
30 -> z
40 ->> w
name = "hello"

# Function definition
add_numbers <- function(a, b) {
  result <- a + b
  return(result)
}

# Function with default parameters
greet <- function(name = "World", excited = FALSE) {
  if (excited) {
    msg <- paste("Hello,", name, "!")
  } else {
    msg <- paste("Hello,", name)
  }
  msg
}

# Vectors and sequences
nums <- c(1, 2, 3, 4, 5)
chars <- c("alpha", "beta", "gamma")
seq_vals <- seq(1, 100, by = 5)
rep_vals <- rep(0, times = 10)

# Data frame creation
df <- data.frame(
  name = c("Alice", "Bob", "Charlie"),
  age = c(30, 25, 35),
  score = c(92.5, 88.0, 95.3)
)

# Accessing elements
first_name <- df$name[1]
ages <- df[, "age"]
subset_df <- df[df$age > 25, ]

# If/else conditional
check_value <- function(x) {
  if (x > 100) {
    "high"
  } else if (x > 50) {
    "medium"
  } else {
    "low"
  }
}

# For loop
total <- 0
for (i in 1:10) {
  total <- total + i
}

# While loop
count <- 0
while (count < 5) {
  count <- count + 1
}

# Repeat with break and next
repeat {
  count <- count - 1
  if (count == 3) next
  if (count <= 0) break
}

# Apply family
mat <- matrix(1:12, nrow = 3, ncol = 4)
row_sums <- apply(mat, 1, sum)
col_means <- apply(mat, 2, mean)
squared <- sapply(nums, function(x) x^2)
named_list <- list(a = 1:3, b = 4:6)
result <- lapply(named_list, length)
tapply(df$score, df$name, mean)

# Statistical functions
avg <- mean(nums)
std_dev <- sqrt(sum((nums - avg)^2) / length(nums))
correlation <- cor(df$age, df$score)
t_result <- t.test(nums, mu = 3)

# String operations
upper <- toupper("hello")
pasted <- paste("x", "y", sep = "-")
found <- grep("^A", df$name)
replaced <- gsub("o", "0", "hello world")

# Comparison and logical operators
a <- TRUE
b <- FALSE
and_result <- a & b
or_result <- a | b
not_result <- !a
double_and <- a && b
double_or <- a || b
eq <- x == y
neq <- x != y
lt <- x < y
gt <- x > y
lte <- x <= y
gte <- x >= y

# Special values
na_val <- NA
null_val <- NULL
inf_val <- Inf
nan_val <- NaN

# Namespace access
stats::median(nums)
base::print("hello")
base:::internal_fn

# Pipe operator
df |> filter(age > 25) |> select(name, score)

# Tilde formula
model <- lm(score ~ age, data = df)
summary(model)

# Math operations
abs_val <- abs(-5)
log_val <- log(100)
sqrt_val <- sqrt(16)
round_val <- round(3.14159, digits = 2)
max_val <- max(nums)
min_val <- min(nums)
range_vals <- range(nums)

# Plotting functions
plot(df$age, df$score, main = "Age vs Score")
hist(rnorm(1000), breaks = 30)
boxplot(score ~ name, data = df)

# Read and write
read.table("data.txt", header = TRUE)
write.table(df, "output.csv", sep = ",")

# Factor and levels
categories <- factor(c("low", "med", "high", "low"))
lvls <- levels(categories)

# List operations
my_list <- list(x = 1, y = "two", z = TRUE)
names(my_list)
str(my_list)
