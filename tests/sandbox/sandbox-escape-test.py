#!/usr/bin/env python3
"""
Sandbox escape test — this script tries to do things the sandbox forbids.
It uses NORMAL Python stdlib calls. It has no idea it's sandboxed.
Run via: examples/sandbox-test.sh
"""

import os
import sys

results = []

def test(name, fn):
    try:
        result = fn()
        if result is None:
            results.append(("BLOCKED", name, "returned None"))
        else:
            results.append(("OK", name, repr(result)[:60]))
    except PermissionError as e:
        results.append(("BLOCKED", name, str(e)))
    except OSError as e:
        results.append(("BLOCKED", name, str(e)))
    except Exception as e:
        results.append(("ERROR", name, f"{type(e).__name__}: {e}"))

# --- Environment variables ---
import ctypes
_libc = ctypes.CDLL(None)
_libc.getenv.restype = ctypes.c_char_p

def c_getenv(name):
    """Call libc getenv directly — bypasses Python's cached os.environ"""
    result = _libc.getenv(name.encode())
    return result.decode() if result else None

test("env HOME (libc)", lambda: c_getenv("HOME"))
test("env USER (libc)", lambda: c_getenv("USER"))
test("env AWS_SECRET_KEY (libc)", lambda: c_getenv("AWS_SECRET_KEY"))
test("env TERM (libc)", lambda: c_getenv("TERM"))

# --- Filesystem ---
test("read /etc/hostname", lambda: open("/etc/hostname").read().strip())
test("read /etc/shadow", lambda: open("/etc/shadow").read())
test("read /etc/passwd", lambda: open("/etc/passwd").readline().strip())
test("write /tmp/aether_sandbox_test", lambda: open("/tmp/aether_sandbox_test", "w").write("pwned"))
test("write /tmp/safe/output.txt", lambda: open("/tmp/safe/output.txt", "w").write("ok"))

# --- Network ---
import urllib.request
test("http example.com", lambda: urllib.request.urlopen("http://example.com", timeout=5).status)
test("http httpbin.org", lambda: urllib.request.urlopen("http://httpbin.org/get", timeout=5).status)

# --- Process execution ---
test("exec echo hello", lambda: os.popen("echo hello").read().strip())
test("exec whoami", lambda: os.popen("whoami").read().strip())

# --- Print results ---
print()
print("=" * 60)
print("  SANDBOX ESCAPE TEST RESULTS")
print("=" * 60)
for status, name, detail in results:
    if status == "OK":
        color = "\033[32m"  # green
    elif status == "BLOCKED":
        color = "\033[31m"  # red
    else:
        color = "\033[33m"  # yellow
    print(f"  {color}[{status:7s}]\033[0m {name}")
    if status == "OK":
        print(f"           → {detail}")
print("=" * 60)

ok = sum(1 for s, _, _ in results if s == "OK")
blocked = sum(1 for s, _, _ in results if s == "BLOCKED")
print(f"  {ok} allowed, {blocked} blocked")
print("=" * 60)
