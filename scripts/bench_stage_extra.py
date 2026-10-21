#!/usr/bin/env python3
"""Add the supplementary 'gatto_big150' topic (first 150 real Animals-10 'gatto'
files, sorted by name) to an already-staged topics root and extend the
provenance CSVs.  150 files < the Enhanced server's 1024 work-unit limit.
"""
import csv
import hashlib
import os
import shutil
import sys

repo, root, results = sys.argv[1:4]
CHUNK = 64 * 1024
src = os.path.join(repo, "raw-img", "gatto")
dst = os.path.join(root, "gatto_big150")
shutil.rmtree(dst, ignore_errors=True)
os.makedirs(dst)
names = sorted(os.listdir(src))[:150]
for n in names:
    shutil.copy2(os.path.join(src, n), os.path.join(dst, n))
sizes = [os.path.getsize(os.path.join(dst, n)) for n in names]
chunks = sum((s + CHUNK - 1) // CHUNK for s in sizes)
with open(os.path.join(results, "dataset_benchmark_topics.csv"), "a", newline="") as f:
    csv.writer(f).writerow(["gatto_big150", "first 150 raw-img/gatto files (sorted by name)",
                            len(sizes), sum(sizes), chunks,
                            "REAL Animals-10 images (not synthetic)"])
with open(os.path.join(results, "dataset_files_sha256.csv"), "a", newline="") as f:
    w = csv.writer(f)
    for n in names:
        p = os.path.join(dst, n)
        h = hashlib.sha256(open(p, "rb").read()).hexdigest()
        w.writerow(["gatto_big150", n, os.path.getsize(p), h])
print("gatto_big150:", len(sizes), "files", sum(sizes), "bytes", chunks, "chunks")
