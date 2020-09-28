#!/usr/bin/env python3
 
import math
import sys
import re
import time
from os import listdir
from os.path import isfile, join
import functools

import matplotlib
# ['GTK3Agg', 'GTK3Cairo', 'MacOSX', 'nbAgg', 'Qt4Agg', 'Qt4Cairo', 'Qt5Agg', 'Qt5Cairo', 'TkAgg', 'TkCairo', 'WebAgg', 'WX', 'WXAgg', 'WXCairo', 'agg', 'cairo', 'pdf', 'pgf', 'ps', 'svg', 'template']
matplotlib.use('pdf')
matplotlib.rcParams['hatch.linewidth'] = 0.5  # previous pdf hatch linewidth
import matplotlib.pyplot as plt
from matplotlib import rc
rc('font', family='serif')
#rc('font', serif=['Times'])
rc('text', usetex=True)

#colors = ['#377eb8', '#ff7f00', '#e41a1c', '#f781bf', '#984ea3', '#999999', '#dede00', '#000055'] #'#4daf4a', '#a65628', '#377eb8']
#markers = ['^', 's', 'o', '+', 'x', '*']
#colors = ['#88bbff', '#ffaaaa', '#ffffaa', '#bbbbbb']
#colors = ['#ffffcc', '#aaccff', '#ffccaa', '#d0d0d0']
colors = ['#ffd6d6', '#d0ddff', '#faffb0', '#e1bfff', '#c0ffb6']

markers = ['^', 's', 'o', '*', 'v', 'p', 'd']
linestyles = ["-.", ":", "--", "-"]
hatches = ["...", "ooo", "/////", " ", "\\\\\\\\\\", "xxxxx"]

"""
/   - diagonal hatching
\\   - back diagonal
|   - vertical
-   - horizontal
+   - crossed
x   - crossed diagonal
o   - small circle
O   - large circle
.   - dots
*   - stars
"""

max_duration = 300

basedir = sys.argv[1]
files = [f for f in listdir(basedir) if isfile(join(basedir, f)) and str(f).startswith("cls_")]

clauses_by_domain = dict()
problems_by_domain = dict()

# For each problem
for file in files:

    match = re.search(r'cls_([0-9]+)_(.*)', file)
    if not match:
        print("[WARN] unparsed file %s" % file)
        continue
    index = int(match.group(1))
    domain = match.group(2)

    print("%i (%s) : %s" % (index, domain, file))
    
    nonempty = False
    for line in open(basedir + "/" + file, 'r').readlines():
        nonempty = True
        words = line.rstrip().split(" ")
        key = words[0]
        val = int(words[1])
        if domain not in clauses_by_domain:
            clauses_by_domain[domain] = dict()
        clauses_by_domain[domain][key] = clauses_by_domain[domain].get(key, 0)+val
    
    if nonempty:
        problems_by_domain[domain] = problems_by_domain.get(domain, 0) + 1



# Aggregate data

clauses = dict()
totals_by_domain = dict()
num_domains = len(clauses_by_domain)

for domain in clauses_by_domain:

    # Count sum of clauses per domain 
    domainsum = 0
    for key in clauses_by_domain[domain]:
        domainsum += clauses_by_domain[domain].get(key, 0)
    # Normalize total by number of problems
    totals_by_domain[domain] = domainsum / problems_by_domain[domain]

    # Normalize clause distribution per domain to sum up to 1
    for key in clauses_by_domain[domain]:
        cls_norm = clauses_by_domain[domain].get(key, 0) / domainsum
        clauses_by_domain[domain][key] = cls_norm
        clauses[key] = clauses.get(key, 0) + cls_norm / num_domains


print(clauses)
for domain in clauses_by_domain:
    print(domain, clauses_by_domain[domain])



# Do plotting

plt.figure(figsize=(6,3.5))

bars = []
labels = []
# Sort domains by their total number of clauses
sorteddomains = sorted(clauses_by_domain) #, key=lambda d: -totals_by_domain[d])
domainlabels = ["Total"] + [d for d in sorteddomains]
Xs = [0.75] + [x for x in range(2, len(sorteddomains)+2)]
plt.xlim(0, len(sorteddomains)+1.75)

plt.axes().set_axisbelow(True)
plt.grid(True, which='major', axis='y')

bottom = [0 for x in range(1, len(sorteddomains)+2)]
for key in sorted(clauses, key=lambda x: -clauses[x]):
    Ys = [clauses[key]] + [clauses_by_domain[d].get(key, 0) for d in sorteddomains]
    print(Xs)
    print(Ys)
    print(bottom)
    bars += [plt.bar(Xs, Ys, width=0.65, bottom=bottom, color=colors[len(labels) % len(colors)],
        hatch=hatches[len(labels) % len(hatches)], alpha=.99)] # alpha trick as workaround that hatching is drawn in PDF
    labels += [key]
    for i in range(len(bottom)):
        bottom[i] += Ys[i]

plt.xticks(Xs, domainlabels, rotation=45, ha='right')
plt.ylabel("Relative clause occurrence")
# Pad margins so that markers don't get clipped by the axes
#plt.margins(0.2)
# Tweak spacing to prevent clipping of tick-labels
#plt.subplots_adjust(bottom=0.15)
#plt.legend(bars, labels)
plt.tight_layout()
#plt.show()
plt.savefig('clause_distribution.pdf')



# Export legend separately

def flip_legend_labels(labels, bars, ncols):

    newlabels = [l for l in labels]
    newbars = [b for b in bars]
    r = 0
    c = 0

    for i in range(len(labels)):
        j = ncols * r + c
        newlabels[i] = labels[j]
        newbars[i] = bars[j]
        if ncols * (r+1) + c >= len(labels):
            r = 0
            c += 1
        else:
            r += 1
    
    return (newlabels, newbars) 

ncols = 4
(newlabels, newbars) = flip_legend_labels(labels, bars, ncols)

figlegend = plt.figure()
figlegend.legend(newbars, newlabels, 'center', ncol=ncols, edgecolor='#000000')
figlegend.set_edgecolor('b')
figlegend.tight_layout()
figlegend.savefig('clause_distribution_legend.pdf', bbox_inches='tight')
