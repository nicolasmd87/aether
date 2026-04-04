#!/usr/bin/env ruby
# Test: Ruby tries to bypass sandbox via Fiddle (dlopen)

# Normal env check
home = ENV['HOME']
puts home ? "  [OK     ] HOME: #{home}" : "  [BLOCKED] HOME"

secret = ENV['AWS_SECRET_KEY']
puts secret ? "  [OK     ] AWS_SECRET_KEY: #{secret}" : "  [BLOCKED] AWS_SECRET_KEY"

# Try to load Fiddle (Ruby's FFI — calls dlopen internally)
begin
  require 'fiddle'
  libc = Fiddle.dlopen("libc.so.6")
  puts "  [OK     ] Fiddle.dlopen libc.so.6: loaded (calls still intercepted)"

  # Try to call getenv through Fiddle
  begin
    getenv = Fiddle::Function.new(
      libc['getenv'],
      [Fiddle::TYPE_VOIDP],
      Fiddle::TYPE_VOIDP
    )
    result = getenv.call("TERM")
    if result.null?
      puts "  [BLOCKED] Fiddle getenv TERM"
    else
      puts "  [OK     ] Fiddle getenv TERM: #{result.to_s} (ESCAPED!)"
    end
  rescue => e
    puts "  [BLOCKED] Fiddle getenv: #{e.message}"
  end
rescue LoadError => e
  puts "  [BLOCKED] require 'fiddle': #{e.message}"
rescue Fiddle::DLError => e
  puts "  [BLOCKED] Fiddle.dlopen libc.so.6: #{e.message}"
rescue => e
  puts "  [BLOCKED] Fiddle: #{e.class}: #{e.message}"
end

# Try File.read on blocked path
begin
  File.read("/etc/shadow")
  puts "  [OK     ] File.read /etc/shadow: ESCAPED!"
rescue => e
  puts "  [BLOCKED] File.read /etc/shadow"
end
