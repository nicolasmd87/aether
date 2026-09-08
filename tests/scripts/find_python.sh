#!/bin/sh
# Print a command that actually runs Python 3, or print nothing and exit 1.
#
#   PY="$(sh tests/scripts/find_python.sh)" || PY=""
#   [ -n "$PY" ] && $PY script.py || echo "  [SKIP] ... no working Python"
#
# Why this is not `command -v python3`:
#
# Windows 10/11 ship "app execution aliases" for python.exe and python3.exe,
# enabled by default, that are NOT Python. Run one and it prints an advert for
# the Microsoft Store and exits 49. `command -v python3` finds it, so every
# guard written that way concludes Python is present and then runs the stub.
# That is how `make ci` came to die at check-docs on a machine with Python 3.13
# installed — installed as python.exe, because the official Windows installer
# ships no python3.exe at all.
#
# So: probe by RUNNING a candidate, and try more than one name. `py -3` is the
# Windows launcher, which is immune to the aliases and is the most reliable
# name on that platform.
#
# Deliberately word-splits `$cand` (py -3 is two words), so callers must use
# it unquoted as `$PY script.py`.
for cand in python3 python "py -3"; do
    if $cand -c "import sys" >/dev/null 2>&1; then
        printf '%s' "$cand"
        exit 0
    fi
done
exit 1
