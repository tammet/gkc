[
{"@id":"pete", "father":"john"},
{"@id":"mark", "father":"pete"},
{"@name":"gfrule",
 "@logic": ["if", {"@id":"?:X","father":"?:Y"}, {"@id":"?:Y","father":"?:Z"}, "then", {"@id":"?:X","grandfather":"?:Z"}]
},
{"@question": {"@id":"?:X","grandfather":"john"}}
]