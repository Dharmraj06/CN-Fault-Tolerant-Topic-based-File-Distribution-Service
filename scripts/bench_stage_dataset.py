#!/usr/bin/env python3
"""Stage the benchmark datasets and record their provenance.

Source : <repo>/raw-img   (Animals-10 images committed to the repository,
         Italian topic names; see Results/dataset_inventory.csv for exact counts)
Target : a topics root on the local Linux filesystem containing
           mucca/          -> byte-exact copy of the real Animals-10 'mucca' topic
           gatto_fault20/  -> 10 real Animals-10 'gatto' files, each in
                              (64 KiB, 128 KiB] i.e. exactly 2 protocol chunks
                              (20 chunks total) used for the fault-rate sweeps.

Why a subset for fault experiments: Case 1 restarts from byte 0 after every
injected fault and the fault decision is taken per 64 KiB chunk, so the
probability that one attempt survives is (1-p)^chunks.  With the full 'mucca'
topic (1866 chunks) a single attempt survives with probability 0.99^1866 ~ 7e-9
even at p = 0.01, i.e. Case 1 can never finish (its client also gives up after
51 attempts).  Using the SAME 20-chunk real-image subset for Case 1, Case 2 and
Enhanced keeps the comparison equivalent and feasible.  The subset is made of
real Animals-10 files and is labelled as such in every result.
"""
import csv
import hashlib
import os
import shutil
import sys

repo, root, results = sys.argv[1], sys.argv[2], sys.argv[3]
CHUNK = 64 * 1024
raw = os.path.join(repo, "raw-img")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def chunks(size):
    return (size + CHUNK - 1) // CHUNK


# ---- inventory of the whole raw-img directory (provenance record) ----------
official_counts = {  # published Animals-10 (Kaggle) class sizes, for comparison
    "cane": 4863, "cavallo": 2623, "elefante": 1446, "farfalla": 2112,
    "gallina": 3098, "gatto": 1668, "mucca": 1866, "pecora": 1820,
    "ragno": 4821, "scoiattolo": 1862,
}
os.makedirs(results, exist_ok=True)
with open(os.path.join(results, "dataset_inventory.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["topic", "files_present", "total_bytes", "published_animals10_count",
                "complete_vs_published", "total_64KiB_chunks", "min_file_bytes",
                "max_file_bytes"])
    for t in sorted(os.listdir(raw)):
        d = os.path.join(raw, t)
        if not os.path.isdir(d):
            continue
        sizes = [os.path.getsize(os.path.join(d, x)) for x in sorted(os.listdir(d))
                 if os.path.isfile(os.path.join(d, x))]
        pub = official_counts.get(t, "")
        w.writerow([t, len(sizes), sum(sizes), pub,
                    ("yes" if pub == len(sizes) else "NO") if pub != "" else "",
                    sum(chunks(s) for s in sizes), min(sizes) if sizes else 0,
                    max(sizes) if sizes else 0])

# ---- stage topics ------------------------------------------------------------
shutil.rmtree(root, ignore_errors=True)
os.makedirs(root)
shutil.copytree(os.path.join(raw, "mucca"), os.path.join(root, "mucca"))

gsrc = os.path.join(raw, "gatto")
cands = []
for x in sorted(os.listdir(gsrc)):
    p = os.path.join(gsrc, x)
    s = os.path.getsize(p)
    if CHUNK < s <= 2 * CHUNK:
        cands.append((x, s))
sel = cands[:10]
os.makedirs(os.path.join(root, "gatto_fault20"))
for x, _ in sel:
    shutil.copy2(os.path.join(gsrc, x), os.path.join(root, "gatto_fault20", x))

with open(os.path.join(results, "dataset_benchmark_topics.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["benchmark_topic", "origin", "file_count", "total_bytes",
                "total_64KiB_chunks", "data_kind"])
    for t, origin in (("mucca", "raw-img/mucca (complete, byte-exact copy)"),
                      ("gatto_fault20", "first 10 raw-img/gatto files with 64KiB<size<=128KiB")):
        d = os.path.join(root, t)
        sizes = [os.path.getsize(os.path.join(d, x)) for x in sorted(os.listdir(d))]
        w.writerow([t, origin, len(sizes), sum(sizes), sum(chunks(s) for s in sizes),
                    "REAL Animals-10 images (not synthetic)"])

with open(os.path.join(results, "dataset_files_sha256.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["benchmark_topic", "filename", "bytes", "sha256"])
    for t in ("mucca", "gatto_fault20"):
        d = os.path.join(root, t)
        for x in sorted(os.listdir(d)):
            p = os.path.join(d, x)
            w.writerow([t, x, os.path.getsize(p), sha256(p)])
print("staged", root, [x for x, _ in sel])
