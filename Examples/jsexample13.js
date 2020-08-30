[
{
  "@id":"smith1",
  "@type":["#:person","baby"],
  "name":"John Smith"  
},
{
  "@id":"smith2",
  "@type":["#:dog","newborn"],  
  "name":"John Smith"  
},
["if", 
  {"@id":"?:X","@type":"?:T1"},
  {"@id":"?:Y","@type":"?:T2"},
  ["?:T1","!=","?:T2"],
  "then", 
  ["?:X","!=","?:Y"]
],
{"@question": ["smith1","!=","smith2"]}
]