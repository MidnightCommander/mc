-- Single line comment
/* Multi-line
   block comment */

-- Data types in CREATE TABLE
CREATE TABLE employees (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    salary DECIMAL(10, 2) DEFAULT 0.00,
    department_id INT,
    hire_date DATE,
    is_active BOOLEAN DEFAULT TRUE,
    bio TEXT,
    data BLOB
);

CREATE TABLE departments (
    id INTEGER PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    budget NUMERIC(15, 2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- INSERT statements
INSERT INTO departments (id, name, budget)
VALUES (1, 'Engineering', 500000.00);

INSERT INTO departments (id, name, budget)
VALUES (2, 'Marketing', 300000.00),
       (3, 'Sales', 250000.00);

INSERT INTO employees (id, name, email, salary, department_id, hire_date)
VALUES (1, 'Alice Johnson', 'alice@example.com', 95000.00, 1, '2020-01-15');

-- SELECT with various clauses
SELECT
    e.id,
    e.name,
    e.salary,
    d.name AS department_name
FROM employees e
INNER JOIN departments d ON e.department_id = d.id
WHERE e.salary > 50000
  AND e.is_active = TRUE
ORDER BY e.salary DESC
LIMIT 10;

-- Aggregate functions and GROUP BY
SELECT
    d.name,
    COUNT(*) AS employee_count,
    AVG(e.salary) AS avg_salary,
    MAX(e.salary) AS max_salary,
    MIN(e.salary) AS min_salary,
    SUM(e.salary) AS total_salary
FROM employees e
LEFT JOIN departments d ON e.department_id = d.id
GROUP BY d.name
HAVING COUNT(*) > 1;

-- UPDATE statement
UPDATE employees
SET salary = salary * 1.10,
    is_active = TRUE
WHERE department_id IN (
    SELECT id FROM departments WHERE name = 'Engineering'
);

-- DELETE statement
DELETE FROM employees
WHERE hire_date < '2015-01-01'
  AND is_active = FALSE;

-- Subquery and EXISTS
SELECT name
FROM employees
WHERE EXISTS (
    SELECT 1
    FROM departments
    WHERE departments.id = employees.department_id
      AND departments.budget > 400000
);

-- CASE expression
SELECT
    name,
    salary,
    CASE
        WHEN salary >= 100000 THEN 'Senior'
        WHEN salary >= 60000 THEN 'Mid'
        ELSE 'Junior'
    END AS level
FROM employees;

-- UNION
SELECT name, 'employee' AS type FROM employees
UNION
SELECT name, 'department' AS type FROM departments;

-- Window functions
SELECT
    name,
    salary,
    RANK() OVER (ORDER BY salary DESC) AS salary_rank
FROM employees;

-- String and comparison operators
SELECT *
FROM employees
WHERE name LIKE '%son'
  AND salary != 0
  AND salary <> 0
  AND salary >= 1000
  AND salary <= 200000
  AND (salary + 1000) / 12 > 5000;

-- ALTER and DROP
ALTER TABLE employees ADD COLUMN notes TEXT;
DROP TABLE IF EXISTS old_employees;

-- CREATE INDEX
CREATE INDEX idx_emp_dept ON employees (department_id);

-- Transactions
BEGIN;
UPDATE employees SET salary = salary + 5000 WHERE id = 1;
COMMIT;
