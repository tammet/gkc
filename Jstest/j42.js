[
{
 "@id": "pete",
 "son": ["john","mark","michael"]
},
{
 "@id": "mark",
 "son": ["pete","ludwig"]
},
{
  "@logic": [
    "and", 
    ["forall",["X","Y"], [{"@id":"X","son":"Y"},"=>",{"@id":"Y","father":"X"}]],
    ["forall",["X","Y","Z"],[
      {"@id":"X","grandfather":"Z"},
      "<=",
      [{"@id":"X","father":"Y"},"&",{"@id":"Y","father":"Z"}]
    ]]      
  ]   
},
{ 
  "@question": {"@id":"john","grandfather":"?:Y"}
}
]