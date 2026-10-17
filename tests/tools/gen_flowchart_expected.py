#!/usr/bin/env python3
"""Derive tests/data/flowchart_expected.tsv from merman's semantic goldens.

Usage: python3 tests/tools/gen_flowchart_expected.py
One line per fixture (tab separated): name, direction, then '|'-joined lists of
nodes (id,label,shape), edges (from,to,label,type,stroke) and subgraphs
(id,title,members joined by ','). Sub-fields are separated by U+001F.
"""
import glob, json, os

root = os.path.join(os.path.dirname(__file__), "..", "data")
US = '\x1f'
rows = []
for f in sorted(glob.glob(os.path.join(root, "merman", "*.golden.json"))):
    if f.endswith(".layout.golden.json"):
        continue
    name = os.path.basename(f)[: -len(".golden.json")]
    m = json.load(open(f))["model"]
    nodes = sorted(US.join([n['id'], n.get('label') or '', n.get('shape') or '']) for n in m["nodes"])
    edges = [US.join([e['from'], e['to'], e.get('label') or '', e['type'], e['stroke']]) for e in m["edges"]]
    subs = sorted(US.join([s['id'], s.get('title') or '', ','.join(sorted(s.get('nodes', [])))]) for s in m["subgraphs"])
    rows.append("\t".join([name, m["direction"], "|".join(nodes), "|".join(edges), "|".join(subs)]))
open(os.path.join(root, "flowchart_expected.tsv"), "w").write("\n".join(rows) + "\n")
print(len(rows), "fixtures")
