%% Sample Erlang module demonstrating syntax highlighting
%% This file exercises all TS captures from erlang-highlights.scm

-module(sample).
-export([start/0, factorial/1, fib/1, process_list/1]).
-compile([export_all, nowarn_export_all]).

%% Records and type definitions
-record(person, {name, age, email}).
-define(MAX_RETRIES, 5).
-define(TIMEOUT, 30000).

%% Function with pattern matching and guards
start() ->
    io:format("Starting sample module~n"),
    Person = #person{name = "Alice", age = 30, email = "a@b.c"},
    Result = factorial(10),
    io:format("Factorial of 10: ~p~n", [Result]),
    List = [1, 2, 3, 4, 5],
    Processed = process_list(List),
    io:format("Processed: ~p~n", [Processed]),
    ok.

%% Recursive function with pattern matching
factorial(0) -> 1;
factorial(N) when N > 0 ->
    N * factorial(N - 1).

%% Fibonacci with multiple clauses
fib(0) -> 0;
fib(1) -> 1;
fib(N) when N > 1 ->
    fib(N - 1) + fib(N - 2).

%% List processing with various constructs
process_list([]) -> [];
process_list([H | T]) ->
    [H * 2 | process_list(T)].

%% Case expression
classify(Value) ->
    case Value of
        0 -> zero;
        N when N > 0 -> positive;
        N when N < 0 -> negative
    end.

%% If expression
check_range(X) ->
    if
        X > 100 -> large;
        X > 10 -> medium;
        X > 0 -> small;
        true -> non_positive
    end.

%% Try-catch with throw
safe_divide(_, 0) ->
    throw(division_by_zero);
safe_divide(A, B) ->
    try
        A / B
    catch
        error:badarith -> {error, arithmetic};
        throw:Reason -> {error, Reason}
    after
        io:format("Division attempted~n")
    end.

%% Receive with timeout
wait_for_message() ->
    receive
        {ping, From} ->
            From ! pong,
            wait_for_message();
        {data, Payload} ->
            process_data(Payload);
        stop ->
            ok
    after ?TIMEOUT ->
        {error, timeout}
    end.

%% Fun (anonymous function)
apply_transform(List) ->
    Doubled = lists:map(fun(X) -> X * 2 end, List),
    Evens = lists:filter(
        fun(X) -> X rem 2 == 0 end,
        List
    ),
    {Doubled, Evens}.

%% List comprehension with generators and filters
comprehension_demo() ->
    Pairs = [{X, Y} || X <- [1, 2, 3],
                        Y <- [a, b, c],
                        X > 1],
    Pairs.

%% Binary and bit syntax
binary_demo() ->
    Bin = <<1, 2, 3, 4>>,
    <<A:8, B:8, _Rest/binary>> = Bin,
    Size = byte_size(Bin),
    {A, B, Size}.

%% Tuple and record operations
record_demo() ->
    P1 = #person{name = "Bob", age = 25, email = "b@c.d"},
    Name = P1#person.name,
    P2 = P1#person{age = 26},
    {Name, P2}.

%% Word operators: and, or, not, div, rem, band, bor, etc.
bitwise_demo(A, B) ->
    R1 = A band B,
    R2 = A bor B,
    R3 = A bxor B,
    R4 = bnot A,
    R5 = A bsl 2,
    R6 = A bsr 1,
    R7 = A div B,
    R8 = A rem B,
    Result = (R1 + R2 + R3 + R4 + R5 + R6) * R7,
    Cond = (A > 0) and (B > 0),
    Other = (A == 0) or (B == 0),
    NotA = not (A =:= 0),
    Check = (A =/= B) andalso (A =< B),
    Final = Check orelse (A >= B),
    {Result, R8, Cond, Other, NotA, Final}.

%% Comparison operators
compare(A, B) ->
    Eq = A == B,
    Neq = A /= B,
    ExEq = A =:= B,
    ExNeq = A =/= B,
    Lt = A < B,
    Gt = A > B,
    Lte = A =< B,
    Gte = A >= B,
    {Eq, Neq, ExEq, ExNeq, Lt, Gt, Lte, Gte}.

%% Characters and strings
string_demo() ->
    Str = "Hello, World!",
    Char = $A,
    NewLine = $\n,
    Atom = hello_world,
    QuotedAtom = 'complex atom name',
    {Str, Char, NewLine, Atom, QuotedAtom}.

%% Spawn and message passing
spawn_demo() ->
    Pid = spawn(fun() -> wait_for_message() end),
    Pid ! {ping, self()},
    receive
        pong -> ok
    end.

%% Begin block
begin_demo(X) ->
    begin
        Y = X + 1,
        Z = Y * 2,
        Z
    end.

%% Internal helper
process_data(Data) ->
    io:format("Data: ~w~n", [Data]),
    {ok, Data}.
