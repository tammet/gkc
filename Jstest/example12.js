[
{
  "@id":"company1",
  "clients": {"@list":["a","b","c"]},
  "revenues": {"@list":[10,20,50,60]}
},

[["listcount","$nil"],"=",0],
[["listcount",["$list","?:X","?:Y"]],"=",["+",1,["listcount","?:Y"]]],

[["listsum","$nil"],"=",0],
[["listsum",["$list","?:X","?:Y"]],"=",["+","?:X",["listsum","?:Y"]]],

["if",
 {"@id":"?:X","clients":"?:C","revenues":"?:R"},
 ["$greater",["listcount","?:C"],2], 
 ["$greater",["listsum","?:R"],100],
 "then", 
 {"@id":"?:X","@type":"goodcompany"} 
],

{"@question": {"@id":"?:X","@type":"goodcompany"}}
 
]
