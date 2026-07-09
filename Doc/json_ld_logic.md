JSON-LD-LOGIC Input Format
==========================

JSON-LD-LOGIC is the primary input format for GK. It embeds logical
formulas in JSON, with optional semantic web compatibility via JSON-LD keys.

Files typically use the `.js` extension and are valid JSON with the
addition of C-style comments (`//` line comments and `/* ... */` block comments).

See also https://github.com/tammet/json-ld-logic for the format proposal.


File Structure
--------------

A GK input file is a JSON array of statements. Each statement is either:

  * A bare formula (JSON array): treated as an axiom with confidence 1.0
  * An annotated formula (JSON object): has metadata keys like `@logic`,
    `@confidence`, `@question`, etc.

Example:

    [
      // bare fact
      ["bird","tweety"],

      // annotated rule with confidence
      {"@logic": [["bird","?:X"], "=>", ["flies","?:X"]],
       "@confidence": 0.9},

      // question
      {"@question": ["flies","tweety"]}
    ]


Top-Level JSON Keys
--------------------

### Formula keys

  * **`@logic`** (array): The logical formula content. Required unless
    `@question` is used.

  * **`@question`** (array): Marks the formula as a question/goal to prove.
    Shorthand for `{"@logic": ..., "@role": "question"}`.

### Metadata keys

  * **`@confidence`** (number): Confidence value for the statement.
    - Float 0.0 to 1.0: direct confidence (e.g., `0.85`)
    - Integer 2 to 100: percentage (e.g., `85` means 0.85)
    - Default: 1.0 (certain)

  * **`@name`** (string): Name identifier for the clause, used in
    proof output.

  * **`@role`** (string): Role of the statement in the problem. Values:
    - `"axiom"`: background knowledge (default if no role specified)
    - `"extaxiom"`: external axiom (from included files)
    - `"assumption"`: assumed true for this problem
    - `"hypothesis"`: hypothesis to test
    - `"conjecture"`: goal to prove (negated internally)
    - `"negated_conjecture"`: already-negated goal
    - `"question"`: question to answer (like conjecture but may seek
      variable bindings)

### Structural keys (JSON-LD compatibility)

  * **`@context`**: JSON-LD context for URI expansion
  * **`@graph`**: Contains a list of nested formula objects
  * **`@id`**: Identifier for JSON-LD objects
  * **`@type`**: Type annotation
  * **`@list`**: Marks a list structure
  * **`@set`**: Marks a set structure
  * **`@base`**: Base URI for relative references

### Include and similarity keys

  * **`@include`** (string): Path to a file to include. The file is parsed
    and its contents added to the problem. Searched in: current directory,
    `$TPTP` environment variable path, `/opt/TPTP/`.

  * **`@similarity`**: Similarity declarations for word-level matching.


Formulas
--------

Formulas are encoded as JSON arrays. The encoding depends on the
logical structure:

### Atoms (atomic formulas)

An atom is a JSON array with a predicate name followed by arguments:

    ["bird","tweety"]              // bird(tweety)
    ["father","john","pete"]       // father(john, pete)
    ["p"]                          // p (propositional)

### Negated atoms

Prefix the predicate name with `-`:

    ["-bird","tweety"]             // not bird(tweety)
    ["-flies","?:X"]               // not flies(X)

### Rules (implications)

Use `=>` as an infix operator in a three-element array:

    [["bird","?:X"], "=>", ["flies","?:X"]]

This is equivalent to the clause `["-bird","?:X"], ["flies","?:X"]`.

Reverse implication:

    [["flies","?:X"], "<=", ["bird","?:X"]]

### Biconditionals

    [["p","?:X"], "<=>", ["q","?:X"]]

### Disjunctions (OR)

A clause (disjunction of literals) is a JSON array of arrays:

    [["-bird","?:X"], ["flies","?:X"]]

This means: `not bird(X) OR flies(X)`.

Explicit OR operator:

    [["p","a"], "|", ["q","b"]]

### Conjunctions (AND)

    [["p","?:X"], "&", ["q","?:X"]]

### Negation

Prefix operators `not`, `~`, or `-` negate a formula:

    ["not", ["flies","?:X"]]
    ["~", ["flies","?:X"]]
    ["-", ["flies","?:X"]]

### Quantifiers

Explicit quantifiers (usually unnecessary since free variables are
implicitly universally quantified in clauses):

    ["forall", ["?:X","?:Y"], [["father","?:X","?:Y"], "=>", ["parent","?:X","?:Y"]]]
    ["exists", ["?:X"], ["bird","?:X"]]

### Conditional (if-then)

    ["if", ["bird","?:X"], "then", ["flies","?:X"]]

With multiple conditions and consequences:

    ["if", ["bird","?:X"], ["small","?:X"], "then", ["canary","?:X"]]


Variables
---------

Variables are strings prefixed with `?:`:

    "?:X"       "?:Y"       "?:Name"       "?:Thing1"

Free variables in a clause are implicitly universally quantified.

Blank nodes (JSON-LD style) use `_:` prefix: `"_:node1"`.


Special Predicates
------------------

### $ans - Answer collection

The `$ans` predicate marks which variable bindings to report:

    ["-bird","?:X"], ["$ans","?:X"]

This asks "what X is a bird?" and reports the bindings found.

In annotated form:

    {"@question": ["-bird","?:X"], "$ans": ["?:X"]}

### $block - Default rule exceptions

Used in default (defeasible) rules to mark exception conditions:

    [["-bird","?:X"], ["flies","?:X"],
     ["$block", strength, ["$not", ["flies","?:X"]]]]

The `$block` literal says: this rule can be overridden if a stronger
rule proves the blocked literal.

**Strength** is the first argument of `$block`:

  * Integer `0`: incomparable with all other blockers
  * Integer `>0`: larger numbers are stronger/more specific
  * `["$", class_number]`: taxonomy-based, using class number
  * `["$", "word"]`: taxonomy-based, word is converted to class number
    via `gk_name_number.txt`
  * `["$", class_or_word, integer]`: combined; uses taxonomy for
    comparison with other taxonomy blockers, integer for comparison
    with integer-only blockers

Blockers with equal strength block each other mutually
(unless strength is 0).

### $not - Negation in blockers

`$not` is used **only** inside `$block` literals to negate the
blocked conclusion:

    ["$block", 1, ["$not", ["flies","?:X"]]]

means "this rule is blocked if flies(X) can be disproved."

    ["$block", 1, ["flies","?:X"]]

means "this rule is blocked if flies(X) can be proved."

### $arc and $narc - RDF triples

For semantic web / RDF data:

    ["$arc", subject, predicate, object]        // unnamed triple
    ["$narc", subject, predicate, object, id]   // named triple

### List operations

  * `["$list", head, tail]`: cons operation
  * `$first`: first element of a list
  * `$rest`: rest of a list (tail)
  * `$nil`: empty list
  * `$is_list`: test if argument is a list

Lists can also be written in bracket notation: `[1, 2, 3]` within terms.

### Arithmetic predicates

  * `$less`, `$greater`, `$lessorequal`, `$greaterorequal`: comparison
  * `$sum`, `$product`, `$difference`, `$quotient`, `$remainder`: operations
  * `$floor`, `$ceiling`, `$round`, `$truncate`: rounding
  * `$is_int`, `$is_real`, `$is_number`: type tests

Infix shorthand: `+`, `-`, `*`, `/` in terms.

### String predicates

  * `$is_distinct`: test if argument is a string (distinct symbol)
  * `$strlen`: string length
  * `$substr`: substring extraction
  * `$substrat`: character at position

### Equality

    ["=", "a", "b"]         // a equals b
    ["!=", "a", "b"]        // a does not equal b

Infix in terms: `["p", ["f","a"], "=", ["f","b"]]`


Logical Operators Summary
--------------------------

| Operator    | Meaning               | Position   |
|-------------|-----------------------|------------|
| `=>`        | Implication           | Infix      |
| `<=>`       | Biconditional         | Infix      |
| `<=`        | Reverse implication   | Infix      |
| `<~>`       | Negated equivalence   | Infix      |
| `&`, `and`  | Conjunction           | Infix      |
| `\|`, `or`  | Disjunction           | Infix      |
| `~\|`       | Negated disjunction   | Infix      |
| `~&`        | Negated conjunction   | Infix      |
| `not`,`~`,`-`| Negation             | Prefix     |
| `=`         | Equality              | Infix      |
| `!=`,`-=`,`~=`| Inequality          | Infix      |
| `forall`    | Universal quantifier  | Prefix     |
| `exists`    | Existential quantifier| Prefix     |
| `if...then` | Conditional           | Keyword    |


Comments
--------

GK's JSON parser supports C-style comments:

    // This is a line comment

    /* This is a
       block comment */

These are extensions to standard JSON, enabled by the YAJL parser.


Distinct Symbols (Strings)
----------------------------

Strings in double quotes are treated as distinct symbols (constants
that are unequal to all other distinct symbols):

    ["name", "\"John Smith\""]

The backslash-escaped quotes are needed because the value is inside
a JSON string.


Complete Example
-----------------

A file combining facts, rules, defaults, and a question:

    [
      // Facts with confidence
      {"@logic": ["bird","tweety"], "@confidence": 0.95},
      {"@logic": ["bird","polly"], "@confidence": 0.9},
      {"@logic": ["penguin","tux"], "@confidence": 0.99},

      // Penguins are birds
      [["penguin","?:X"], "=>", ["bird","?:X"]],

      // Default: birds fly (strength 1)
      {"@logic": [["-bird","?:X"], ["flies","?:X"],
                   ["$block", 1, ["$not", ["flies","?:X"]]]]},

      // Exception: penguins don't fly (strength 2, overrides birds)
      {"@logic": [["-penguin","?:X"], ["-flies","?:X"],
                   ["$block", 2, ["flies","?:X"]]]},

      // Question: what flies?
      {"@question": ["-flies","?:X"], "$ans": ["?:X"]}
    ]

Running:

    ./gk example.js -defaults

Expected output: tweety and polly fly (with their respective confidences),
but tux does not fly (the penguin exception overrides the bird default).
