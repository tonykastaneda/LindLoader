"""Exact-text replace that keeps a file's line endings (works on LF, CRLF
and mixed files). Usage (as module): sub(path, old, new)"""
def sub(p, a, b, cnt=1):
    raw = open(p, 'rb').read().decode('utf-8')
    if a not in raw and '\r\n' in raw:
        a = a.replace('\n', '\r\n'); b = b.replace('\n', '\r\n')
    assert a in raw, (p, a[:80])
    open(p, 'wb').write(raw.replace(a, b, cnt).encode('utf-8'))
