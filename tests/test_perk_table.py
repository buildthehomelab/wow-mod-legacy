#!/usr/bin/env python3
"""Checks the perk table in src/Legacy.cpp against the Perk enum in src/Legacy.h.

PERKS is indexed by the enum, so its rows must come in the enum's order. The launcher draws each
tree as a grid of 3 columns and 4 rows (it clamps anything outside that), so no two perks of a
tree may share a cell, and the General strip is one row.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src")


def read(name):
    with open(os.path.join(SRC, name), encoding="utf-8") as f:
        return f.read()


def main():
    header = read("Legacy.h")
    enum = re.search(r"enum Perk : uint8\s*\{(.*?)MAX_PERKS", header, re.S).group(1)
    names = re.findall(r"^\s*(PERK_\w+),", enum, re.M)

    source = read("Legacy.cpp")
    table = re.search(r"std::array<PerkInfo, MAX_PERKS> const PERKS =\s*\{\{(.*?)\}\};", source, re.S).group(1)
    rows = re.findall(r"\{\s*(PERK_\w+),\s*\"(\w+)\",\s*(TREE_\w+),\s*\"[^\"]+\",\s*\"\w+\",\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(PERK_\w+|NO_PERK),", table)

    problems = []
    if [r[0] for r in rows] != names:
        problems.append("PERKS rows are not in the enum's order:\n  enum:  %s\n  table: %s" % (names, [r[0] for r in rows]))

    keys = [r[1] for r in rows]
    if len(set(keys)) != len(keys):
        problems.append("duplicate perk keys")

    trees = {r[0]: r[2] for r in rows}
    cells = {}
    for perk, key, tree, max_rank, row, column, _points, requires in rows:
        row, column, max_rank = int(row), int(column), int(max_rank)
        if not 1 <= max_rank <= 20:
            problems.append("%s: max rank %d outside 1-20 (the portal and launcher limit)" % (perk, max_rank))
        if row > 3 or column > 2 or (tree == "TREE_GENERAL" and row != 0):
            problems.append("%s: row %d column %d is outside the launcher's grid" % (perk, row, column))
        if (tree, row, column) in cells:
            problems.append("%s and %s share %s row %d column %d" % (cells[(tree, row, column)], perk, tree, row, column))
        cells[(tree, row, column)] = perk
        if requires != "NO_PERK" and trees.get(requires) != tree:
            problems.append("%s requires %s from another tree" % (perk, requires))

    if problems:
        print("\n".join(problems))
        sys.exit(1)
    print("Perk table: %d perks, in enum order, one per launcher cell" % len(rows))


if __name__ == "__main__":
    main()
