
import sys
import re

corefile = sys.argv[1]
varmapfile = sys.argv[2]

names = dict()

for line in open(varmapfile, 'r').readlines():
    line = line.rstrip()
    match = re.search(r'VARMAP (-?[0-9]+) (.*)', line)
    if match:
        var = match.group(1)
        name = match.group(2)
        names[var] = name
        #print(var,"->",name)

for line in open(corefile, 'r').readlines():
    line = line.rstrip()
    if "p cnf" in line:
        continue
    words = line.split(" ")
    out = ""
    for word in words:
        if len(word) > 0 and word[0] == '-':
            out += "-"
            word = word[1:]
        if word in names:
            out += names[word] + " "
        else:
            out += word + " "
    print(out)
