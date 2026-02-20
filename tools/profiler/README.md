# Aether Profiler

Web-based profiler dashboard for real-time monitoring of Aether runtime performance.

## Features

- **Memory Profiling**: Track allocations, deallocations, and memory leaks
- **Actor Tracing**: Monitor actor spawning, message passing, and lifecycle
- **Real-time Metrics**: Live dashboard with auto-refresh
- **Event Timeline**: Detailed event log with filtering
- **JSON Export**: Download profiling data for offline analysis

## Quick Start

### Build the Profiler

From the project root:

```bash
make profiler
```

### Run the Demo

```bash
./build/profiler_demo
```

Then open your browser to: **http://localhost:8081**

## Integration

### 1. Link the profiler in your project

Add to your build:

```makefile
PROFILER_SRC = tools/profiler/profiler_server.c
CFLAGS += -DAETHER_PROFILING
```

### 2. Initialize in your code

```c
#include "tools/profiler/profiler_server.h"

int main() {
    ProfilerConfig config = {
        .enabled = 1,
        .port = 8081,
        .max_events = 10000
    };
    
    profiler_init(&config);
    profiler_start_server();
    
    // Your application code...
    
    profiler_shutdown();
    return 0;
}
```

### 3. Record events

```c
ProfilerEvent event = {
    .type = PROF_EVENT_ACTOR_MESSAGE_SENT,
    .timestamp_ms = get_current_time_ms(),
    .actor_id = sender_id,
    .target_actor_id = receiver_id,
    .message_type = msg_type
};
profiler_record_event(&event);
```

## API Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /` | Dashboard HTML page |
| `GET /api/metrics` | Current metrics snapshot (JSON) |
| `GET /api/events?count=N` | Recent events (JSON) |
| `GET /api/export` | Full data export (JSON) |
| `POST /api/clear` | Clear event history |

## Configuration

```c
typedef struct {
    int enabled;               // Enable profiling
    int port;                  // Server port (default: 8081)
    const char* bind_address;  // Bind address (default: "0.0.0.0")
    int max_events;            // Event buffer size (default: 10000)
    int collection_interval_ms; // Metrics collection interval
} ProfilerConfig;
```

## Dashboard Features

### Metric Cards
- **Memory Usage**: Current and peak memory consumption
- **Active Allocations**: Live allocation count
- **Active Actors**: Number of running actors
- **Message Throughput**: Messages sent/processed

### Event Log
- Real-time event stream
- Color-coded event types
- Timestamp and details for each event
- Auto-scroll to latest events

### Actions
- **Export JSON**: Download all profiling data
- **Clear Events**: Reset event buffer

## Performance Impact

The profiler is designed to have minimal overhead:
- Ring buffer prevents unbounded memory growth
- Lock-free event recording (mutex only for ring buffer)
- Metrics aggregation happens on request, not during recording
- HTTP server runs in separate thread

**Overhead**: ~1-3% CPU, ~2MB RAM per 10,000 events

## Disabling the Profiler

To disable profiling in production:

1. Don't define `-DAETHER_PROFILING` in CFLAGS
2. Set `config.enabled = 0`
3. Don't call `profiler_start_server()`

## Examples

See `profiler_demo.c` for a complete example demonstrating:
- Profiler initialization
- Event recording
- Actor simulation
- Memory tracking integration

## Troubleshooting

**Port already in use:**
```
Failed to bind to port 8081
```
Solution: Change port in config or kill process using port 8081

**Can't connect to dashboard:**
- Check firewall settings
- Ensure program is still running
- Try http://127.0.0.1:8081 instead of localhost

**No events showing:**
- Verify `config.enabled = 1`
- Check that events are being recorded with `profiler_record_event()`
- Ensure profiler was initialized before events

## Future Enhancements

- WebSocket support for true real-time updates
- Flamegraph visualization
- Actor message flow diagrams
- Performance regression detection
- Integration with Chrome DevTools Protocol

