{
  "@id":"b",
  "f":"c",
  "@logic": [
    "and",
    [["$arc","?:X","f","?:Y"],"=>",["f","?:X","?:Y"]],
    ["f","c","d"],
    [["gf","?:X","?:Z"],"<=",[["f","?:X","?:Y"],"&",["f","?:Y","?:Z"]]],
    ["-gf","b","d"]
  ]
}