[
["father","john","pete"],
["father","pete","mark"],
["forall", ["X","Y","Z"], [[["father","X","Y"], "&", ["father","Y","Z"]], "=>", ["grandfather","X","Z"]]],

// negated question: does there exist somebody who has john as a grandfather?
["forall", ["X"], ["not",["grandfather","john","X"]]]
]
