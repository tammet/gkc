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
    [{"@id":"?:X","grandfather":"?:Z"},
     "<=",
     [{"@id":"?:X","father":"?:Y"},"&",{"@id":"?:Y","father":"?:Z"}]]  
},
{   
  "@question": {"@id":"john","grandfather":"?:Y"}
}
]
