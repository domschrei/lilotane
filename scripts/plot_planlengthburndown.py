#!/usr/bin/env python3
 
import math
import sys
import re
from os import listdir
from os.path import isfile, join

import matplotlib.pyplot as plt
from matplotlib import rc
#rc('font',**{'family':'sans-serif','sans-serif':['Helvetica']})
## for Palatino and other serif fonts use:
#rc('font',**{'family':'serif','serif':['Palatino']})
rc('text', usetex=True)

colors = ['#377eb8', '#ff7f00', '#e41a1c', '#f781bf', '#a65628', '#4daf4a', '#984ea3', '#999999', '#dede00', '#377eb8']
#markers = ['^', 's', 'o', '+', 'x', '*']
markers = ['^', 's', 'o', '*', 'v', 'p']
linestyles = ["-.", ":", "--", "-"]

basedir = sys.argv[1]
files = [f for f in listdir(basedir) if isfile(join(basedir, f)) and str(f).startswith("plo_")]

style_by_domain = dict()
ci = 0
mi = 0
li = 0

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
    optlength = 0

    # Parse plan length updates
    for line in open(basedir + "/" + file, 'r').readlines():
        words = line.rstrip().split(" ")
        timestamp = float(words[0])
        action = words[2]
        length = int(words[3])
        if not Xs:
            begin_of_plo = timestamp
        Xs += [timestamp/begin_of_plo - 1] # directly normalize x values
        Ys += [length]
        if action == "BEGIN":
            initlength = length
        if action == "END":
            optpoint = [timestamp/begin_of_plo - 1, length]
    
    # Normalize y values
    if initlength == optlength:
        Ys = [0 for y in Ys]
        if optpoint:
            optpoint[1] = 0
    else:
        Ys = [1/(initlength-optlength)*y - optlength/(initlength-optlength) for y in Ys]
        if optpoint:
            optpoint[1] = 1/(initlength-optlength)*optpoint[1] - optlength/(initlength-optlength)

    # Plot planlength burndown for this problem
    plt.plot(Xs, Ys, c=style[0], marker=style[1], linestyle=style[2], fillstyle='none')
    if optpoint:
        # Depth-optimal plan proven: marker of increased size
        plt.plot([optpoint[0]], [optpoint[1]], c=style[0], marker=style[1])


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