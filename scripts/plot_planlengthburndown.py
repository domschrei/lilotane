#!/usr/bin/env python3
 
import math
import sys
import re
from os import listdir
from os.path import isfile, join
import functools

import matplotlib.pyplot as plt
from matplotlib import rc
#rc('font',**{'family':'sans-serif','sans-serif':['Helvetica']})
## for Palatino and other serif fonts use:
#rc('font',**{'family':'serif','serif':['Palatino']})
rc('text', usetex=True)

colors = ['#377eb8', '#ff7f00', '#e41a1c', '#f781bf', '#984ea3', '#999999', '#dede00', '#000055'] #'#4daf4a', '#a65628', '#377eb8']
#markers = ['^', 's', 'o', '+', 'x', '*']
markers = ['^', 's', 'o', '*', 'v', 'p', 'd']
linestyles = ["-.", ":", "--", "-"]

max_duration = 300

basedir = sys.argv[1]
files = [f for f in listdir(basedir) if isfile(join(basedir, f)) and str(f).startswith("plo_")]

style_by_domain = dict()
problems_by_domain = dict()
ci = 0
mi = 0
li = 0

# Compare two problem tuples
def compare(p1, p2):
    if p1[3] < p2[3]:
        return -1
    elif p1[3] > p2[3]:
        return 1
    else:
        return 0

# For each problem
for file in files:

    match = re.search(r'plo_([0-9]+)_(.*)', file)
    if not match:
        print("[WARN] unparsed file %s" % file)
        continue
    index = int(match.group(1))
    domain = match.group(2)

    print("%i (%s) : %s" % (index, domain, file))
    
    # Retrieve style for the problem's domain
    if domain not in style_by_domain:
        style_by_domain[domain] = (colors[ci], markers[mi], linestyles[li])
        ci = (ci+1) % len(colors)
        mi = (mi+1) % len(markers)
        li = (li+1) % len(linestyles)
    style = style_by_domain[domain]

    Xs = []
    Ys = []
    optpoint = []
    initlength = 0
    optlength = -1
    begin_of_plo = -1
    duration = max_duration

    # Parse plan length updates
    for line in open(basedir + "/" + file, 'r').readlines():
        words = line.rstrip().split(" ")
        timestamp = float(words[0])
        action = words[2]
        length = int(words[3])
        if begin_of_plo == -1:
            begin_of_plo = timestamp
        else:
            Xs += [timestamp/begin_of_plo - 0.999] # directly normalize x values
            #Xs += [timestamp]
            Ys += [length]
        if action == "BEGIN":
            initlength = length
        if action == "END":
            optpoint = [timestamp/begin_of_plo - 0.999, length]
            #optpoint = [timestamp, length]
            duration = timestamp
            optlength = length
    #if optlength == -1:
    optlength = 0
    
    # Normalize y values
    if initlength == optlength:
        Ys = [0 for y in Ys]
        if optpoint:
            optpoint[1] = 0
    else:
        Ys = [1/(initlength-optlength)*y - optlength/(initlength-optlength) for y in Ys]
        if optpoint:
            optpoint[1] = 1/(initlength-optlength)*optpoint[1] - optlength/(initlength-optlength)

    # Update problems per domain
    if domain not in problems_by_domain:
        problems_by_domain[domain] = []
    problems_by_domain[domain] += [(Xs, Ys, optpoint, duration)]


# Plot problems per domain
for domain in problems_by_domain:
    problems = sorted(problems_by_domain[domain], key=functools.cmp_to_key(compare))
    # Pick (up to) three slowest problems
    indices = [len(problems)-1, len(problems)-2, len(problems)-3]
    # Pick all problems
    indices = range(len(problems))
    for i in indices:
        if i < 0 or i >= len(problems):
            continue
        
        (Xs, Ys, optpoint, duration) = problems[i]
        style = style_by_domain[domain]
        plt.plot(Xs, Ys, c=style[0], marker=style[1], linestyle=style[2], fillstyle='none')
        if optpoint:
            # Depth-optimal plan proven: filled marker
            plt.plot([optpoint[0]], [optpoint[1]], c=style[0], marker=style[1])


# Line for problems which were not completely optimized
#plt.plot([-80,80], [0.5,0.5], c='#555555', marker='', linestyle="--")

# Construct legend
for domain in sorted(style_by_domain):
    style = style_by_domain[domain]
    plt.plot([], [], c=style[0], marker=style[1], linestyle=style[2], label=domain)
plt.legend()

# Log scale?
#plt.xscale("log")

# Labels
plt.xlabel("$\\frac{\\textnormal{Elapsed time since } T_0}{T_0}$")
plt.ylabel("Relative plan length $\\frac{|\\pi|}{|\\pi_0|}$")

plt.show()