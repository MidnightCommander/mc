"A Smalltalk sample file for syntax highlighting comparison"
"Exercises TS captures from smalltalk-highlights.scm"

"Class definition with instance variables"
Object subclass: #Animal
    instanceVariableNames: 'name age sound'
    classVariableNames: 'Count'
    poolDictionaries: ''
    category: 'Examples'.

"Method definition with temporaries and assignment"
Animal >> initialize
    | temp |
    temp := 'default'.
    name := temp.
    age := 0.
    sound := 'none'.
    Count := Count + 1.

"Method with return value"
Animal >> name
    ^ name.

"Method with keyword messages"
Animal >> setName: aName age: anAge
    name := aName.
    age := anAge.

"Unary message sends"
Animal >> describe
    | result |
    result := name size.
    ^ result printString.

"Binary operators"
Animal >> isOlderThan: other
    ^ age > (other age).

"Keyword message with multiple parts"
Animal >> feedAt: aTime with: aFood
    | msg |
    msg := 'Feeding ' , name , ' at ' , aTime printString.
    msg := msg , ' with ' , aFood.
    ^ msg.

"Cascades with semicolons"
Animal >> setup
    self
        setName: 'Rex' age: 5;
        sound := 'bark'.

"Block with arguments"
Animal >> doTimes: aBlock
    | i |
    i := 1.
    [i <= age] whileTrue: [
        aBlock value: i.
        i := i + 1.
    ].

"Boolean literals and control flow"
Animal >> isValid
    | valid |
    valid := true.
    (name = nil) ifTrue: [valid := false].
    (age < 0) ifTrue: [valid := false].
    ^ valid.

"Using self and super"
Animal >> copy
    | clone |
    clone := super copy.
    clone setName: name age: age.
    ^ clone.

"Collection operations"
Animal >> allNames: aCollection
    | names |
    names := OrderedCollection new.
    aCollection do: [:each |
        names add: each name.
    ].
    ^ names.

"Symbols and characters"
Animal >> typeSymbol
    ^ #animal.

Animal >> firstChar
    ^ $A.

"Numeric literals"
Animal >> compute
    | x y |
    x := 42.
    y := 3.14.
    ^ x + y.

"Nested blocks"
Animal >> deepBlock
    [:a |
        [:b |
            [:c | a + b + c]
        ]
    ].

"Class method with new"
Animal class >> createNamed: aName
    | inst |
    inst := self new.
    inst setName: aName age: 1.
    ^ inst.

"Error handling keywords"
Animal >> safeDo: aBlock
    | result |
    result := [aBlock value]
        on: Error
        do: [:ex | ex return: nil].
    ^ result.

"Using thisContext"
Animal >> currentMethod
    ^ thisContext method.
!
