[
{
  "@id":"b",
  "f":"?:Z",
  "p": {
    "r": "?:Z",
    "m": {
      "k": 2,
      "@logic": ["forall",["X"],["exists",["Y"],["mm","X","Y","?:Z"]]]
    }
  } 
    
},
{ 
  "@role":"question",
  "@logic": [["$arc","?:X","p","?:Y"],"&",["$arc","?:Y","r","?:Z"],"&",["$arc","b","f","?:Z"]]
}
]