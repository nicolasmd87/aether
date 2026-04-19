"""Round-trip an Aether lib through SWIG-generated Python bindings."""
import sys

import aether_lib

# Primitive round-trip: aether_add(2, 40) -> 42
result = aether_lib.aether_add(2, 40)
assert result == 42, f"aether_add(2, 40) = {result}, expected 42"

# String round-trip
s = aether_lib.aether_shout("hello")
assert s == "hello", f"aether_shout('hello') = {s!r}, expected 'hello'"

print("OK: SWIG Python bindings round-trip primitive and string values")
sys.exit(0)
