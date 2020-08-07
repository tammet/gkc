[
{ "@context": {
    "@base": "http://people.org/",
    "@vocab":"http://bar.org/", 
    "father":"http://foo.org/father"
  },
  "@id":"pete", 
  "father":"john",
  "son": ["mark","michael"],
  "@logic": ["forall",["X","Y"],[{"@id":"X","son":"Y"}, "=>", {"@id":"Y","father":"X"}]]
},      
["if", {"@id":"?:X","http://foo.org/father":"?:Y"}, {"@id":"?:Y","http://foo.org/father":"?:Z"}, "then", {"@id":"?:X","grandfather":"?:Z"}],
{"@question": {"@id":"?:X","grandfather":"john"}}
]