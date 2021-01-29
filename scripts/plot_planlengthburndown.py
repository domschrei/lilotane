#!/usr/bin/env python3
 
import math
import sys
import re
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

colors = ['#377eb8', '#ff7f00', '#e41a1c', '#f781bf', '#984ea3', '#999999', '#dede00', '#000055'] #'#4daf4a', '#a65628', '#377eb8']
#markers = ['^', 's', 'o', '+', 'x', '*']
markers = ['^', 's', 'o', '*', 'v', 'p', 'd']
linestyles = ["-.", ":", "--", "-", ":"]

max_duration = 300

basedir = sys.argv[1]
files = [f for f in listdir(basedir) if isfile(join(basedir, f)) and str(f).startswith("plo_")]

style_by_domain = dict()
problems_by_domain = dict()
improvements_by_domain = dict()
runtimes_by_domain = dict()
efficiencies_by_domain = dict()



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

    Xs = []
    Ys = []
    optpoint = []
    initlength = 0
    optlength = -1
    begin_of_plo = -1
    duration = max_duration
    efficiency = None

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
            #if timestamp == begin_of_plo:
            #    efficiency = 6000
            #elif length != initlength or timestamp-begin_of_plo >= 1:
            efficiency = [timestamp-begin_of_plo, (initlength-length)/initlength]
        
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
    if Xs:
        if domain not in problems_by_domain:
            problems_by_domain[domain] = []
        problems_by_domain[domain] += [(Xs, Ys, optpoint, duration)]

    # Add total relative improvement and total relative runtime to dicts 
    if Xs:
        if domain not in improvements_by_domain:
            improvements_by_domain[domain] = []
            runtimes_by_domain[domain] = []
            efficiencies_by_domain[domain] = []
        improvements_by_domain[domain] += [1 - Ys[-1]]
        runtimes_by_domain[domain] += [Xs[-1]]
        efficiencies_by_domain[domain] += [efficiency]


# Compare two problem tuples of shape (Xs, Ys, [optX,optY], duration)
def compare(p1, p2):
    if p1[3] < p2[3]:
        return -1
    elif p1[3] > p2[3]:
        return 1
    else:
        return 0

# Collect problems per domain
problems = []
domains = sorted([d for d in problems_by_domain])
for domain in domains:

    # Create and retrieve style for the problem's domain
    if domain not in style_by_domain:
        style_by_domain[domain] = (colors[ci], markers[mi], linestyles[li])
        ci = (ci+1) % len(colors)
        mi = (mi+1) % len(markers)
        li = (li+1) % len(linestyles)
    style = style_by_domain[domain]

    # Problem shape: (Xs, Ys, [optX,optY], duration)

    # Pick most optimized problem
    most_opt_problems = sorted(problems_by_domain[domain], key=lambda x: x[1][-1])
    if most_opt_problems:
        problems += [[most_opt_problems[0], style]]
    
    # Pick problem taking the longest
    longest_problems = sorted(problems_by_domain[domain], key=lambda x: x[0][-1])
    if longest_problems:
        problems += [[longest_problems[-1], style]]






# Burndown diagram

plt.figure(figsize=(6,3.5))

# Plot all problems descendingly by their final Y coordinate
for [(Xs, Ys, optpoint, duration), style] in sorted(problems, key=lambda x: x[0][1][-1]):
    plt.plot(Xs, Ys, c=style[0], marker=style[1], linestyle=style[2], lw=1, fillstyle='none')
for [(Xs, Ys, optpoint, duration), style] in sorted(problems, key=lambda x: x[0][1][-1]):
    if optpoint:
        # Depth-optimal plan proven: filled marker
        plt.plot([optpoint[0]], [optpoint[1]], c=style[0], marker=style[1])

plt.xlabel("$\\frac{\\textnormal{Elapsed time since } T_0}{T_0}$")
plt.ylabel("Relative plan length $\\frac{|\\pi|}{|\\pi_0|}$")
#plt.ylim([0,1])
plt.tight_layout()
plt.savefig("planlength_burndown.pdf")
plt.close()




# Efficiency scatter plot

plt.figure(figsize=(6,3.5))
for domain in domains:
    style = style_by_domain[domain]
    e = efficiencies_by_domain[domain]
    plt.plot([x for [x,y] in e], [y for [x,y] in e], c=style[0], marker=style[1], linestyle='none', fillstyle='none', label=domain)
plt.xscale("log")
#plt.yscale("log")
plt.xlabel("Elapsed time since $T_0$ / s")
plt.ylabel("Ratio of actions eliminated")
plt.tight_layout()
plt.savefig("planlength_efficiency.pdf")
plt.close()




# Export legend (for burndown and efficiency) separately

def flip_legend_labels(labels, plots, ncols):

    newlabels = [l for l in labels]
    newplots = [b for b in plots]
    r = 0
    c = 0

    for i in range(len(labels)):
        j = ncols * r + c
        newlabels[i] = labels[j]
        newplots[i] = plots[j]
        if ncols * (r+1) + c >= len(labels):
            r = 0
            c += 1
        else:
            r += 1
    
    return (newlabels, newplots) 

plots = []
for domain in domains:
    style = style_by_domain[domain]
    plots += [matplotlib.lines.Line2D([], [], c=style[0], marker=style[1], linestyle=style[2], lw=1, fillstyle='none', label=domain)]
figlegend = plt.figure()
ncols = 1
(newlabels, newplots) = flip_legend_labels(domains, plots, ncols)
print(newlabels)
figlegend.legend(newplots, newlabels, 'center', ncol=ncols, edgecolor='#000000')
figlegend.set_edgecolor('b')
figlegend.tight_layout()
figlegend.savefig('planlength_legend.pdf', bbox_inches='tight')




# Box plots of total improvement and total runtime per domain

impr = []
runt = []
for domain in domains:
    impr += [improvements_by_domain[domain]]
    runt += [runtimes_by_domain[domain]]

plt.figure(figsize=(6,3.5))
plt.boxplot(impr)
plt.xticks([x for x in range(1, len(domains)+1)], domains, rotation=45, ha='right')
plt.ylabel("Relative plan improvement")
plt.tight_layout()
plt.savefig("planlength_improvements.pdf")
plt.close()

plt.figure(figsize=(6,3.5))
plt.boxplot(runt)
plt.xticks([x for x in range(1, len(domains)+1)], domains, rotation=45, ha='right')
plt.ylabel("Optimization time relative to $T_0$")
plt.tight_layout()
plt.savefig("planlength_runtimes.pdf")
plt.close()

