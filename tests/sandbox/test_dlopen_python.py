#!/usr/bin/env python3
"""Test: Python tries to bypass sandbox via ctypes (dlopen libc)"""
import os

results = []

# Normal env check (should be sandboxed by LD_PRELOAD)
import ctypes
_libc = ctypes.CDLL(None)
_libc.getenv.restype = ctypes.c_char_p

home = _libc.getenv(b"HOME")
results.append(("HOME", home.decode() if home else "BLOCKED"))

secret = _libc.getenv(b"AWS_SECRET_KEY")
results.append(("AWS_SECRET_KEY", secret.decode() if secret else "BLOCKED"))

# Try to load libc directly via ctypes — this calls dlopen
try:
    libc = ctypes.CDLL("libc.so.6")
    libc.getenv.restype = ctypes.c_char_p
    term = libc.getenv(b"TERM")
    results.append(("ctypes libc.getenv TERM", term.decode() if term else "BLOCKED"))
except OSError as e:
    results.append(("ctypes dlopen libc.so.6", f"BLOCKED: {e}"))

# Try to call raw open() via ctypes
try:
    libc = ctypes.CDLL("libc.so.6")
    fd = libc.open(b"/etc/shadow", 0)  # O_RDONLY=0
    if fd >= 0:
        results.append(("ctypes open /etc/shadow", "ESCAPED!"))
        libc.close(fd)
    else:
        results.append(("ctypes open /etc/shadow", "BLOCKED"))
except OSError as e:
    results.append(("ctypes dlopen for open()", f"BLOCKED: {e}"))

for name, val in results:
    status = "BLOCKED" if "BLOCKED" in str(val) else "OK"
    color = "\033[31m" if status == "BLOCKED" else "\033[32m"
    print(f"  {color}[{status:7s}]\033[0m {name}: {val}")
