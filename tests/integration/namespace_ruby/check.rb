# End-to-end check for the generated Ruby SDK.
#
# Loads the SDK module that `ae build --namespace` emitted, exercises
# every input setter, every event handler, and every script function,
# and asserts the round-trip behavior.

require_relative 'calc_generated_sdk'

def fail!(msg)
  warn "FAIL: #{msg}"
  exit 1
end

ns = CalcGeneratedSdk::Calc.new(ARGV[0])

# --- discovery ---
m = ns.describe
fail!("namespace = #{m.namespace_name.inspect}") unless m.namespace_name == 'calc'
fail!("inputs = #{m.inputs.inspect}") unless m.inputs.map(&:first) == ['limit', 'name']
fail!("events = #{m.events.inspect}") unless m.events.map(&:first) == ['Computed', 'Overflow']
fail!("ruby_module = #{m.ruby_module.inspect}") unless m.ruby_module == 'calc_generated_sdk'

# --- setters (v1: stored, not pushed to script) ---
ns.set_limit(100)
ns.set_name('paul')
fail!("setters didn't store on the instance") unless ns.limit == 100 && ns.name == 'paul'

# --- event handlers (snake_case method names; PascalCase event in manifest) ---
computed_ids = []
overflow_ids = []
ns.on_computed { |id| computed_ids << id }
ns.on_overflow { |id| overflow_ids << id }

# --- function calls ---
r = ns.double_it(7)
fail!("double_it(7) = #{r}") unless r == 14
fail!("after double_it, computed_ids = #{computed_ids.inspect}") unless computed_ids == [7]

r = ns.double_it(2_000_000)
fail!("double_it(2_000_000) = #{r} (expected -1 for overflow)") unless r == -1
fail!("after overflow, overflow_ids = #{overflow_ids.inspect}") unless overflow_ids == [2_000_000]

r = ns.label('hello', 42)
fail!("label('hello', 42) = #{r.inspect}") unless r == 'hello'
fail!("label didn't fire Computed (got #{computed_ids.inspect})") unless computed_ids.last == 42

fail!("is_positive(5) = #{ns.is_positive(5)}")  unless ns.is_positive(5) == 1
fail!("is_positive(-5) = #{ns.is_positive(-5)}") unless ns.is_positive(-5) == 0

puts "OK: Ruby SDK round-trip — discovery, setters, events, functions"
