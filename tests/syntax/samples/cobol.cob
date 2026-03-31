      * Sample COBOL program demonstrating syntax highlighting
      * This file exercises all TS captures from cobol-highlights.scm

       IDENTIFICATION DIVISION.
       PROGRAM-ID. SAMPLE-PROGRAM.
       AUTHOR. TEST-AUTHOR.
       DATE-WRITTEN. 2024-01-15.

       ENVIRONMENT DIVISION.
       CONFIGURATION SECTION.
       SOURCE-COMPUTER. LINUX.
       OBJECT-COMPUTER. LINUX.

       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT INPUT-FILE ASSIGN TO "input.dat"
               ORGANIZATION IS SEQUENTIAL
               ACCESS MODE IS SEQUENTIAL
               FILE STATUS IS WS-FILE-STATUS.

       DATA DIVISION.
       FILE SECTION.
       FD INPUT-FILE.
       01 INPUT-RECORD PIC X(80).

       WORKING-STORAGE SECTION.
      * Level numbers -> @string.special
       01 WS-FILE-STATUS    PIC XX VALUE SPACES.
       01 WS-COUNTER        PIC 9(5) VALUE ZEROS.
       01 WS-TOTAL          PIC 9(7)V99 VALUE 0.
       01 WS-NAME           PIC X(30) VALUE "John Doe".
       01 WS-AMOUNT         PIC S9(7)V99 VALUE -1234.56.
       01 WS-DATE           PIC 9(8) VALUE 20240115.

      * Group items with various PIC patterns
       01 WS-EMPLOYEE-REC.
           05 WS-EMP-ID     PIC 9(6).
           05 WS-EMP-NAME   PIC A(25).
           05 WS-EMP-DEPT   PIC X(10).
           05 WS-EMP-SALARY PIC 9(7)V99.

      * Level 88 conditions -> @string.special
       01 WS-STATUS         PIC 9 VALUE 0.
           88 STATUS-ACTIVE     VALUE 1.
           88 STATUS-INACTIVE   VALUE 0.
           88 STATUS-PENDING    VALUE 2 THRU 5.

      * Numeric edited PIC -> @string.special
       01 WS-DISPLAY-AMT    PIC $ZZZ,ZZ9.99-.
       01 WS-EDIT-DATE      PIC 99/99/9999.

       PROCEDURE DIVISION.
       MAIN-PARAGRAPH.
      * I/O operations -> various highlights
           OPEN INPUT INPUT-FILE
           READ INPUT-FILE
               AT END
                   DISPLAY "End of file reached"
               NOT AT END
                   DISPLAY "Record: " INPUT-RECORD
           END-READ

      * String operations
           STRING WS-EMP-NAME DELIMITED BY SPACES
                  " - "       DELIMITED BY SIZE
                  WS-EMP-DEPT DELIMITED BY SPACES
                  INTO WS-NAME
           END-STRING

      * Arithmetic with various verbs
           ADD 100 TO WS-COUNTER
           SUBTRACT 50 FROM WS-TOTAL
           MULTIPLY WS-AMOUNT BY 2
               GIVING WS-TOTAL
           DIVIDE WS-TOTAL BY 3
               GIVING WS-AMOUNT REMAINDER WS-COUNTER

           COMPUTE WS-TOTAL =
               WS-AMOUNT * 1.05 + 100

      * Control flow
           IF WS-COUNTER > 1000
               DISPLAY "Counter exceeds limit"
           ELSE
               DISPLAY "Counter: " WS-COUNTER
           END-IF

           EVALUATE TRUE
               WHEN STATUS-ACTIVE
                   DISPLAY "Status is active"
               WHEN STATUS-INACTIVE
                   DISPLAY "Status is inactive"
               WHEN OTHER
                   DISPLAY "Status unknown"
           END-EVALUATE

           PERFORM PROCESS-RECORDS
               VARYING WS-COUNTER FROM 1 BY 1
               UNTIL WS-COUNTER > 10

           MOVE WS-AMOUNT TO WS-DISPLAY-AMT
           DISPLAY "Formatted: " WS-DISPLAY-AMT

      * Hex and null-terminated strings
           MOVE X"48454C4C4F" TO WS-NAME
           MOVE N"Unicode text" TO WS-NAME

           CLOSE INPUT-FILE
           STOP RUN.

       PROCESS-RECORDS.
           DISPLAY "Processing record " WS-COUNTER
           ADD 1 TO WS-TOTAL.
