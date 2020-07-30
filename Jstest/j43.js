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
    [{"@id":"?:X","son":"?:Y"},"=>",{"@id":"?:Y","father":"?:X"}],
    [
     {"@id":"?:X","grandfather":"?:Z"},
      "<=",
     [{"@id":"?:X","father":"?:Y"},"&",{"@id":"?:Y","father":"?:Z"}]
    ]      
  ]   
},
{ 
  "@role": "question",
  "@logic": {"@id":"john","grandfather":"?:Y"}
}
]