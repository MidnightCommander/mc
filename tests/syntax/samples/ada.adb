-- Ada syntax sample: demonstrates all TS capture groups
-- This file exercises every capture in ada-highlights.scm

-- General keywords (@keyword -> yellow)
with Ada.Text_IO; use Ada.Text_IO;
with Ada.Integer_Text_IO;
pragma Elaborate_All (Ada.Text_IO);

-- Definition keywords (@constant.builtin -> brightmagenta)
package body Demo_Package is

   -- Declaration keywords (@property -> brightcyan)
   type Color is (Red, Green, Blue);
   type Matrix is array (1 .. 3, 1 .. 3) of Float;
   subtype Small_Int is Integer range 0 .. 100;

   -- Type-like keywords (@label -> cyan)
   type Node;
   type Node is record
      Value : Integer;
      Next  : access Node;
   end record;

   type Shape is tagged limited null record;
   type Abstract_Shape is abstract tagged null record;

   -- Control flow keywords (@function.special -> brightred)
   procedure Process (X : in out Integer) is
      Result : Integer := 0;
   begin
      if X > 0 then
         for I in 1 .. X loop
            Result := Result + I;
            exit when Result > 100;
         end loop;
      elsif X = 0 then
         Result := 1;
      else
         while X < 0 loop
            X := X + 1;
         end loop;
      end if;

      case Result is
         when 0 =>
            Put_Line ("Zero");
         when 1 .. 10 =>
            Put_Line ("Small");
         when others =>
            Put_Line ("Large");
      end case;

      declare
         Temp : Integer := Result;
      begin
         Result := Temp * 2;
      exception
         when others =>
            Result := 0;
      end;
   end Process;

   -- Function definition (@constant.builtin -> brightmagenta)
   function Add (A, B : Integer) return Integer is
   begin
      return A + B;
   end Add;

   -- Generic (@constant.builtin -> brightmagenta)
   generic
      type Element is private;
   package Stack is
      procedure Push (E : in Element);
   end Stack;

   -- Task (@keyword -> yellow)
   task type Worker is
      entry Start;
   end Worker;

   task body Worker is
   begin
      accept Start do
         null;
      end Start;
      select
         delay 1.0;
      end select;
   end Worker;

   -- Protected type
   protected type Counter is
      procedure Increment;
      function Get return Integer;
   private
      Count : Integer := 0;
   end Counter;

   -- Keywords: abort, raise, separate, renames, reverse
   procedure Error_Handler is
   begin
      raise Constraint_Error;
   end Error_Handler;

   -- Operators (@string.special -> brightgreen)
   procedure Operators is
      A : Integer := 10;
      B : Integer := 3;
      C : Boolean;
      D : Float := 2.0;
   begin
      A := A + B;
      A := A - B;
      A := A * B;
      A := A / B;
      D := D ** 2;
      A := A mod B;
      A := abs A;
      C := (A = B);
      C := (A /= B);
      C := (A < B);
      C := (A > B);
      C := (A <= B);
      C := (A >= B);
      C := not C;
      C := C and True;
      C := C or False;
      C := C xor True;
   end Operators;

   -- String literals (@string -> green)
   S1 : String := "Hello, Ada!";
   S2 : String := "Line one" & "Line two";

   -- Character literals (@string.special -> brightgreen)
   Ch1 : Character := 'A';
   Ch2 : Character := ' ';

   -- Labels (@label -> cyan)
   procedure Goto_Demo is
   begin
      <<Start_Label>>
      goto Start_Label;
   end Goto_Demo;

   -- Delimiters (@delimiter -> brightcyan)
   -- Covered by: . , : ; ( )

   -- Comments (@comment -> brown)
   -- This is a line comment

   -- Overriding keyword (@property -> brightcyan)
   overriding procedure Initialize;

   -- Access, all, at, constant, goto, interface
   type Ptr is access all Integer;
   I : constant Integer := 42;

end Demo_Package;
