[
{"@id":"pete", "father":"john"},
{"@id":"mark", "father":"pete"},
{"@name":"gfrule",
  "@logic": ["if", ["$arc","?:X","father","?:Y"],["$arc","?:Y","father","?:Z"], "then", ["$arc","?:X","grandfather","?:Z"]]
},
{"@question": ["$arc","?:X","grandfather","john"]}
]