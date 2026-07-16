JSON-LD-LOGIC Input Format
==========================

JSON-LD-LOGIC is GKC's JSON input format. It embeds classical first-order
formulas in JSON, with optional semantic-web compatibility via JSON-LD keys.

Files typically use the `.js` extension and are valid JSON with the
addition of C-style comments (`//` line comments and `/* ... */` block comments).

See also https://github.com/tammet/json-ld-logic for the format proposal.


File Structure
--------------

A GKC input file is a JSON array of statements. Each statement is either:

  * A bare formula (JSON array), treated as an axiom
  * An annotated formula (JSON object), using keys such as `@logic`,
    `@name`, `@role`, and `@question`

Example:

    [
      // bare fact
      ["bird","tweety"],

      // named rule
      {"@name": "birds_fly",
       "@logic": [["bird","?:X"], "=>", ["flies","?:X"]]},

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

  * **`@name`** (string): Name identifier for the clause, used in
    proof output.

  * **`@role`** (string): Role of the statement in the problem. Values:
    - `"axiom"`: background knowledge (default if no role specified)
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
  * **`@base`**: Base URI for relative references when used inside `@context`

### Include key

  * **`@include`** (string): Path to a file to include. The file is parsed
    and its contents added to the problem. Searched in: current directory,
    `$TPTP` environment variable path, `/opt/TPTP/`.

GKC is a classical prover. GK-only confidence annotations, default-rule
blockers, and similarity metadata are outside this format reference and are
not interpreted here as GK's nonclassical mechanisms.


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

    [["bird","?:X"], "=>", ["$ans","?:X"]]

This asks "what X is a bird?" and reports the bindings found.

The convenience `@question` form constructs the corresponding answer query:

    {"@question": ["bird","?:X"]}

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

  * `$less`, `$greater`, `$lesseq`, `$greatereq`: comparison
  * `$sum`, `$product`, `$difference`, `$quotient`, `$remainder`: operations
  * `$floor`, `$ceiling`, `$round`, `$truncate`: rounding
  * `$is_int`, `$is_real`, `$is_number`: type tests

Infix shorthand: `+`, `-`, `*`, `/` in terms.

### String predicates

  * `$is_distinct`: test if argument is a string (distinct symbol)
  * `$strlen`: string length
  * `$substr(A,B)`: test whether distinct symbol A is a substring of B
  * `$substrat(A,B,N)`: test whether A occurs in B at zero-based position N

### Equality

    ["a", "=", "b"]         // a equals b
    ["a", "!=", "b"]        // a does not equal b


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

GKC's JSON parser supports C-style comments:

    // This is a line comment

    /* This is a
       block comment */

These are extensions to standard JSON, enabled by the YAJL parser.


Distinct Symbols (Strings)
----------------------------

JSON strings prefixed with `#:` are treated as distinct symbols (typed
constants unequal to syntactically different distinct symbols):

    ["kind", "#:person"]


Complete Example
-----------------

A file combining facts, a rule, and a question:

    [
      ["bird","tweety"],
      {"@name": "birds_fly",
       "@logic": [["bird","?:X"], "=>", ["flies","?:X"]]},
      {"@question": ["flies","tweety"]}
    ]

Running:

    ./gkc example.js -parallel 0

Expected result: `proof found`.
