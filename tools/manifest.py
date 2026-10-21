"""Print "<md5> <size> <relative path>" for every file under a folder."""
import hashlib, os, sys
root = sys.argv[1]
for d, _, fs in os.walk(root):
    for f in fs:
        p = os.path.join(d, f)
        h = hashlib.md5()
        with open(p, 'rb') as fh:
            for b in iter(lambda: fh.read(1 << 20), b''):
                h.update(b)
        print(h.hexdigest(), os.path.getsize(p), os.path.relpath(p, root).replace(os.sep, '/'))
