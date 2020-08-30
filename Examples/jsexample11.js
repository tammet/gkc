[
{ 
  "@id":"bobknows",
  "@graph": [  
    {"@id":"pete", "father":"john"}
  ]
},    
{ 
  "@id":"eveknows",
  "@graph": [  
    {"@id":"mark", "father":"pete"}
  ]
}, 
{
  "@name":"mergegraphs",
  "@logic": [["$narc","?:X","?:Y","?:Z","?:U"],"=>",["$arc","?:X","?:Y","?:Z"]]
},  
{
  "@name":"gfrule",
  "@logic": ["if", {"@id":"?:X","father":"?:Y"}, {"@id":"?:Y","father":"?:Z"}, "then", {"@id":"?:X","grandfather":"?:Z"}]
},      
{"@question": {"@id":"?:X","grandfather":"john"}}
]  
