[
{ "@context": {    
    "@vocab":"http://foo.org/",   
    "age":"hasage"
  },
  "father":"john",
  "child": [
    {"age": 10}, 
    {"age": 2} 
  ],      
  "@logic": ["and",    
     ["forall",["X","Y"],[{"@id":"X","child":"Y"}, "<=>", ["or",{"@id":"Y","father":"X"},{"@id":"Y","mother":"X"}]]],     
     ["forall",["X","Y"],[{"@id":"Y","child":"X"}, "<=>", ["or",{"@id":"Y","son":"X"},{"@id":"Y","daughter":"X"}]]]
  ]  
},      
["if", {"@id":"?:X","http://foo.org/hasage":"?:Y"}, ["$less",6,"?:Y"], ["$less","?:Y",19], "then", {"@id":"?:X", "@type": "schoolchild"}],
["if", {"@id":"?:X","http://foo.org/father":"?:Y"}, {"@id":"?:Y","http://foo.org/father":"?:Z"}, "then", {"@id":"?:X","grandfather":"?:Z"}],
["if", {"@id":"?:X","http://foo.org/mother":"?:Y"}, {"@id":"?:Y","http://foo.org/father":"?:Z"}, "then", {"@id":"?:X","grandfather":"?:Z"}],
{"@question": [{"@id":"?:X","grandfather":"john"}, "&", {"@id":"?:X","@type":"schoolchild"}]}
]
