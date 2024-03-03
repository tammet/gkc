#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# Call on the command line like ./rdiff.py base_parallel4.txt sharedunits_parallel4.txt

import sys, csv



def main():
  if not sys.argv: return
  if len(sys.argv)<3: return
  args=sys.argv[1:]
  print(args)
  all_lines=[]
  all_spl_lines=[]
  count=0
  for fname in args:
    cfile=open(fname,"r")
    lines=cfile.readlines()
    cfile.close()
    all_lines.append(lines)
    line_spls=[]
    for el in lines:
      spl=el.split(",")
      line_spls.append(spl)
    all_spl_lines.append(line_spls)  
    count+=1
    
  left=all_spl_lines[0]
  right=all_spl_lines[1]
  lmore=0
  rmore=0  
  for lel in left:
    name=lel[0]
    lres=lel[1]
    for rel in right:
      if rel[0]!=name: continue
      rres=rel[1]
      if lres!=rres:
        if lres=="Proved": lmore+=1
        if rres=="Proved": rmore+=1
        print(",".join(lel).strip())
        print(",".join(rel).strip())
        print()
  print("leftmore:  ",lmore)
  print("rightmore: ",rmore)  
  return      

# ---- run top level ----

main()

# ============== docs ==================

