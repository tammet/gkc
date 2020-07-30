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
    ["forall",["X","Y","Z"],[{"@id":"X","grandfather":"Z"},"<=",[{"@id":"X","father":"Y"},"&",{"@id":"Y","father":"Z"}]]]  
},
{ 
  "@question": ["$arc","john","grandfather","?:Y"]
}
]