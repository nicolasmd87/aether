"""End-to-end check for the generated Python SDK.

Loads the SDK module that `ae build --namespace` emitted, exercises
every input setter, every event handler, and every script function,
and asserts the round-trip behavior.
"""
import sys
import calc_sdk

def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)

ns = calc_sdk.Calc(sys.argv[1])

# --- discovery ---
m = ns.describe()
if m.namespace_name != "calc":
    fail(f"namespace_name = {m.namespace_name}")
if [n for n, _ in m.inputs] != ["limit", "name"]:
    fail(f"inputs = {m.inputs}")
if [n for n, _ in m.events] != ["Computed", "Overflow"]:
    fail(f"events = {m.events}")
if m.python_module != "calc_sdk":
    fail(f"python_module = {m.python_module}")

# --- setters (v1: stored, not pushed to script) ---
ns.set_limit(100)
ns.set_name("paul")
if ns.limit != 100 or ns.name != "paul":
    fail("setters didn't store on the instance")

# --- event handlers ---
computed_ids = []
overflow_ids = []
ns.on_Computed(lambda i: computed_ids.append(i))
ns.on_Overflow(lambda i: overflow_ids.append(i))

# --- function calls ---
r = ns.double_it(7)
if r != 14:
    fail(f"double_it(7) = {r}")
if computed_ids != [7]:
    fail(f"after double_it, computed_ids = {computed_ids}")

r = ns.double_it(2_000_000)
if r != -1:
    fail(f"double_it(2_000_000) = {r} (expected -1 for overflow)")
if overflow_ids != [2_000_000]:
    fail(f"after overflow, overflow_ids = {overflow_ids}")

r = ns.label("hello", 42)
if r != "hello":
    fail(f"label('hello', 42) = {r!r}")
if computed_ids[-1] != 42:
    fail(f"label didn't fire Computed (got {computed_ids})")

r = ns.is_positive(5)
if r != 1: fail(f"is_positive(5) = {r}")
r = ns.is_positive(-5)
if r != 0: fail(f"is_positive(-5) = {r}")

print("OK: Python SDK round-trip — discovery, setters, events, functions")
