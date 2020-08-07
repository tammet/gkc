[
["father","john","eve"],
["mother","eve","mark"],
["forall", ["X","Y","Z"], [
  [[["father","X","Y"], "&", ["father","Y","Z"]],
   "|", 
   [["father","X","Y"], "&", ["mother","Y","Z"]]],
  "<=>", 
  ["grandfather","X","Z"]]
],
["forall", ["X"], ["not",["grandfather","john","X"]]]
]