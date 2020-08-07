[
{
  "name":"John Smith",
  "age":20.1,
  "mother": "mary"
},
{
  "name":"John Smith",
  "age":20.1,
  "mother": "eve"
},

["if", {"@id":"?:X","name":"?:N","age":"?:A"},{"@id":"?:Y","name":"?:N","age":"?:A"}, "then", ["?:X","=","?:Y"]],
["if", {"@id":"?:X","mother":"?:M1"},{"@id":"?:X","mother":"?:M2"}, "then", ["?:M1","=","?:M2"]],

{"@question": ["mary","=","eve"]}
]