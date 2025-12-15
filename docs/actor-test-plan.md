# Actor Implementation - Test Plan

## Phase 1: Basic Syntax (Current)

### Test 1: Struct Generation ✅
**File**: `examples/test_actor_working.ae`

```aether
actor Counter {
    state int count = 0;
    receive(msg) {
        count = count + 1;
    }
}
```

**Expected C Output**:
```c
typedef struct Counter {
    int id;
    int active;
    Mailbox mailbox;
    int count;
} Counter;

void Counter_step(Counter* self) {
    Message msg;
    if (!mailbox_receive(&self->mailbox, &msg)) {
        self->active = 0;
        return;
    }
    (count = (count + 1));  // TODO: needs self->count
}
```

**Status**: Struct generates ✅, Assignment generates ✅, State access needs fixing 🚧

### Test 2: Multiple State Variables
```aether
actor Player {
    state int health = 100;
    state int score = 0;
    receive(msg) {
        score = score + 10;
    }
}
```

### Test 3: Message Type Handling
```aether
actor Counter {
    state int count = 0;
    receive(msg) {
        if (msg.type == 1) {
            count = count + 1;
        }
    }
}
```

## Phase 2: Message Passing

### Test 4: Send Function
```aether
actor Pinger {
    receive(msg) {
        send(msg.sender, 42);
    }
}
```

### Test 5: Actor Spawning
```aether
main() {
    Counter c = spawn Counter();
    send(c, Message { type: 1 });
}
```

## Phase 3: Scheduler Integration

### Test 6: Multiple Actors
```aether
main() {
    Counter c1 = spawn Counter();
    Counter c2 = spawn Counter();
    
    send(c1, Message { type: 1 });
    send(c2, Message { type: 1 });
    
    run_scheduler();  // Process all messages
}
```

### Test 7: Ring Benchmark
```aether
// Create N actors in ring, pass token M times
actor Node {
    state int next_id;
    receive(msg) {
        if (msg.type == TOKEN) {
            send(next_id, msg);
        }
    }
}
```

## Test Execution

```bash
# Compile test
./build/aetherc examples/test_actor_working.ae output.c

# Check generated C
cat output.c

# Try to compile C (will fail until state access fixed)
gcc -c output.c -Iruntime
```

## Success Criteria

**Phase 1 Complete When**:
- [x] Actor struct generates with correct fields
- [x] Step function generates with mailbox receive
- [x] Assignments parse and generate correctly
- [ ] State variables generate with self-> prefix
- [ ] Generated C compiles without errors

**Phase 2 Complete When**:
- [ ] send() function works
- [ ] Message struct initialization works
- [ ] Actor spawning works
- [ ] Multiple actors can be created

**Phase 3 Complete When**:
- [ ] Scheduler runs actors
- [ ] Messages are delivered
- [ ] Ring benchmark runs
- [ ] Performance matches POC (125M msg/s)

## Current Blockers

1. **State variable access** - `count` needs to become `self->count` in step function
2. **Mailbox header** - Need to include `actor_state_machine.h` in generated code
3. **Message initialization** - No syntax yet for creating messages
4. **Actor spawning** - No runtime function for allocating actors
