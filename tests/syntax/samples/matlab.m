% MATLAB sample: demonstrate all syntax features

% Function definition
function result = fibonacci(n)
    if n <= 1
        result = n;
    else
        result = fibonacci(n - 1) + fibonacci(n - 2);
    end
end

% Class definition
classdef MyClass
    properties
        Name string
        Value double = 0
    end

    methods
        function obj = MyClass(name, val)
            obj.Name = name;
            obj.Value = val;
        end

        function disp(obj)
            fprintf('%s: %f\n', obj.Name, obj.Value);
        end
    end

    events
        DataChanged
    end

    enumeration
        Red, Green, Blue
    end
end

% Control flow
for i = 1:10
    if mod(i, 2) == 0
        continue;
    elseif i > 7
        break;
    end
    disp(i);
end

% While loop
x = 100;
while x > 1
    x = x / 2;
end

% Switch statement
color = 'red';
switch color
    case 'red'
        disp('Red');
    case 'blue'
        disp('Blue');
    otherwise
        disp('Other');
end

% Try/catch
try
    result = 1 / 0;
catch e
    disp(e.message);
end

% Parallel for
parfor i = 1:100
    data(i) = sqrt(i);
end

% Matrix operations
A = [1, 2, 3; 4, 5, 6; 7, 8, 9];
B = A';
C = A * B;
D = A .* B;
E = A ./ B;
F = A .^ 2;

% Cell array
cells = {'hello', 42, [1 2 3]};
cells{1}

% Struct
s.name = 'test';
s.value = 3.14;

% Logical operators
if (x > 0) && (x < 100)
    flag = true;
elseif (x <= 0) || (x >= 100)
    flag = false;
end

% Comparison operators
a = (1 == 1);
b = (1 ~= 2);
c = (3 > 2);
d = (1 < 2);
e = (2 >= 2);
f = (1 <= 2);

% String types
str1 = 'single quoted string';
str2 = "double quoted string";
str3 = sprintf('formatted: %d %s %.2f', 42, 'hello', 3.14);

% Global and persistent
global sharedVar;
persistent cachedData;

% Function arguments block
function out = validate(x, y)
    arguments
        x (1,1) double {mustBePositive}
        y (1,1) double = 1
    end
    out = x + y;
end

% Comments
% Line comment
%{
   Block comment
   spanning multiple lines
%}

% Escape sequences in strings
msg = sprintf('line1\nline2\ttab\\backslash');

% SPMD block
spmd
    data = labindex * 10;
end
