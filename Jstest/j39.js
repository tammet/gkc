[
{
 "@id": "john",
 "father": "pete"
},
{
 "@id": "pete",
 "father": "mark"
},
{
  "@logic": 
    ["forall",["X","Y","Z"],[["$arc","X","grandfather","Z"],"<=",[["$arc","X","father","Y"],"&",["$arc","Y","father","Z"]]]]  
},
{ 
  "@role": "question",
  "@logic": ["$arc","john","grandfather","?:Y"]
}
]