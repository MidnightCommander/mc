{ Sample file demonstrating Pascal syntax highlighting features. }
{ Exercises all captures from the TS query override. }

program SyntaxDemo;

uses
  SysUtils, Classes;

const
  MAX_SIZE = 100;
  PI_VALUE = 3.14159;
  GREETING = 'Hello, World!';
  IS_ACTIVE = True;
  NOTHING = nil;

type
  TColor = (Red, Green, Blue);
  TIntArray = array[0..9] of Integer;
  TName = string[50];

  TAnimal = class
  private
    FName: string;
    FAge: Integer;
  public
    constructor Create(AName: string; AAge: Integer);
    destructor Destroy; override;
    property Name: string read FName write FName;
    property Age: Integer read FAge write FAge;
    function Speak: string; virtual;
  end;

  TDog = class(TAnimal)
  public
    function Speak: string; override;
  end;

  TPoint = record
    X, Y: Integer;
  end;

  TShape = object
    Width, Height: Integer;
    function Area: Integer;
  end;

var
  I, J, K: Integer;
  S: string;
  Arr: TIntArray;
  Dog: TDog;
  P: TPoint;
  Colors: set of TColor;

{ Constructor implementation }
constructor TAnimal.Create(AName: string; AAge: Integer);
begin
  inherited Create;
  FName := AName;
  FAge := AAge;
end;

{ Destructor implementation }
destructor TAnimal.Destroy;
begin
  inherited Destroy;
end;

function TAnimal.Speak: string;
begin
  Result := 'Generic animal sound';
end;

function TDog.Speak: string;
begin
  Result := 'Woof!';
end;

{ Regular function }
function Add(A, B: Integer): Integer;
begin
  Result := A + B;
end;

{ Procedure }
procedure PrintMessage(Msg: string);
begin
  WriteLn(Msg);
end;

{ Function with local variables }
function Factorial(N: Integer): Integer;
var
  Temp: Integer;
begin
  if N <= 1 then
    Result := 1
  else
    Result := N * Factorial(N - 1);
end;

{ Main program }
begin
  { Arithmetic operators }
  I := 10 + 5;
  J := 10 - 3;
  K := 4 * 2;
  I := 10 div 3;
  J := 10 mod 3;
  S := 10 / 3;

  { Comparison operators }
  if I = J then
    WriteLn('equal');
  if I <> J then
    WriteLn('not equal');
  if I < J then
    WriteLn('less');
  if I > J then
    WriteLn('greater');
  if I <= J then
    WriteLn('less or equal');
  if I >= J then
    WriteLn('greater or equal');

  { Logical operators -> cyan }
  if (I > 0) and (J > 0) then
    WriteLn('both positive');
  if (I > 0) or (J > 0) then
    WriteLn('at least one positive');
  if not (I = 0) then
    WriteLn('not zero');
  if (I > 0) xor (J > 0) then
    WriteLn('exactly one positive');

  { Bit shift operators }
  I := J shl 2;
  I := J shr 1;

  { Type checking }
  if Dog is TAnimal then
    WriteLn('is animal');
  S := (Dog as TAnimal).Speak;

  { Set operations }
  Colors := [Red, Green];
  if Blue in Colors then
    WriteLn('has blue');

  { Control flow }
  if I > 0 then
    WriteLn('positive')
  else
    WriteLn('non-positive');

  { Case statement }
  case I of
    0: WriteLn('zero');
    1: WriteLn('one');
    2..5: WriteLn('small');
  else
    WriteLn('big');
  end;

  { For loop }
  for I := 0 to 9 do
    Arr[I] := I * I;

  for I := 9 downto 0 do
    WriteLn(Arr[I]);

  { While loop }
  I := 0;
  while I < 10 do
  begin
    I := I + 1;
  end;

  { Repeat/until loop }
  I := 10;
  repeat
    I := I - 1;
  until I = 0;

  { With statement }
  P.X := 10;
  P.Y := 20;
  with P do
    WriteLn(X, ', ', Y);

  { Try/except/finally }
  try
    I := 10 div 0;
  except
    WriteLn('Division by zero');
  end;

  try
    Dog := TDog.Create('Rex', 5);
    WriteLn(Dog.Speak);
  finally
    Dog.Free;
  end;

  { Raise exception }
  try
    raise Exception.Create('Test error');
  except
    WriteLn('Caught');
  end;

  { Boolean constants }
  if True then
    WriteLn('true');
  if not False then
    WriteLn('not false');

  { String type keyword }
  S := string('hello');

  { Delimiters -> lightgray }
  WriteLn(Add(3, 4));

  WriteLn('Done.');
end.

(* Alternative comment style *)
// Line comment
