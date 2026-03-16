-- wrk Lua script: simulate typical MQTT-over-HTTP workloads
-- Payloads mimic real IoT sensor data, device commands, telemetry

local counter = 0

-- Typical MQTT message patterns
local messages = {
  -- Sensor telemetry (most common, ~60% of traffic)
  '{"topic":"sensors/temp/device-001","qos":0,"payload":{"temperature":23.5,"humidity":61.2,"timestamp":1710500000}}',
  '{"topic":"sensors/temp/device-002","qos":0,"payload":{"temperature":19.8,"humidity":72.1,"timestamp":1710500001}}',
  '{"topic":"sensors/pressure/device-003","qos":1,"payload":{"pressure":1013.25,"altitude":152.3,"timestamp":1710500002}}',
  '{"topic":"sensors/motion/device-004","qos":0,"payload":{"accel_x":0.02,"accel_y":-0.98,"accel_z":0.01,"gyro_x":0.1,"gyro_y":-0.05,"gyro_z":0.0}}',
  '{"topic":"sensors/gps/vehicle-101","qos":1,"payload":{"lat":-34.6037,"lon":-58.3816,"speed":42.5,"heading":275,"satellites":12}}',
  '{"topic":"sensors/energy/meter-050","qos":0,"payload":{"voltage":220.3,"current":15.2,"power":3348.6,"energy_kwh":1523.7}}',

  -- Device status heartbeats (~20% of traffic)
  '{"topic":"devices/status/device-001","qos":0,"payload":{"online":true,"uptime":86400,"firmware":"2.1.3","rssi":-67}}',
  '{"topic":"devices/status/device-002","qos":0,"payload":{"online":true,"uptime":172800,"firmware":"2.1.3","rssi":-52}}',
  '{"topic":"devices/battery/device-004","qos":1,"payload":{"level":73,"charging":false,"voltage":3.82,"estimated_hours":48}}',

  -- Commands/control messages (~10% of traffic)
  '{"topic":"commands/device-001/set","qos":2,"payload":{"action":"set_threshold","temperature_max":30.0,"temperature_min":15.0}}',
  '{"topic":"commands/device-003/reboot","qos":2,"payload":{"action":"reboot","delay_seconds":5}}',

  -- Alerts (~10% of traffic)
  '{"topic":"alerts/critical/device-002","qos":2,"payload":{"type":"temperature_high","value":45.2,"threshold":40.0,"message":"Temperature exceeds safe limit"}}',
  '{"topic":"alerts/warning/meter-050","qos":1,"payload":{"type":"power_spike","value":5200.0,"threshold":5000.0,"duration_ms":350}}'
}

function request()
  counter = counter + 1
  local idx = (counter % #messages) + 1
  local body = messages[idx]

  -- Simulate different MQTT API endpoints
  local paths = {
    "/api/mqtt/publish",
    "/api/mqtt/publish",
    "/api/mqtt/publish",
    "/api/mqtt/publish",
    "/api/mqtt/publish",
    "/api/mqtt/publish",
    "/api/mqtt/telemetry",
    "/api/mqtt/telemetry",
    "/api/mqtt/status",
    "/api/mqtt/command",
    "/api/mqtt/command",
    "/api/mqtt/alert",
    "/api/mqtt/alert"
  }
  local path = paths[idx]

  return wrk.format("POST", path, {
    ["Content-Type"] = "application/json",
    ["X-MQTT-Topic"] = "sensors/temp/device-001",
    ["X-MQTT-QoS"] = "0"
  }, body)
end

function done(summary, latency, requests)
  local msg_per_sec = summary.requests / (summary.duration / 1000000)
  local avg_latency = latency.mean / 1000
  local max_latency = latency.max / 1000
  local p99_latency = latency:percentile(99.0) / 1000

  io.write("\n--- MQTT Workload Summary ---\n")
  io.write(string.format("  Messages/sec:  %.0f\n", msg_per_sec))
  io.write(string.format("  Avg latency:   %.2f ms\n", avg_latency))
  io.write(string.format("  P99 latency:   %.2f ms\n", p99_latency))
  io.write(string.format("  Max latency:   %.2f ms\n", max_latency))
  io.write(string.format("  Total msgs:    %d\n", summary.requests))
  io.write(string.format("  Errors:        %d\n", summary.errors.status + summary.errors.connect + summary.errors.timeout))

  -- Estimate avg payload size
  local total_size = 0
  for _, m in ipairs(messages) do total_size = total_size + #m end
  local avg_size = total_size / #messages
  local throughput_mb = (msg_per_sec * avg_size) / (1024 * 1024)
  io.write(string.format("  Avg payload:   %.0f bytes\n", avg_size))
  io.write(string.format("  Data rate:     %.2f MB/s\n", throughput_mb))
end
