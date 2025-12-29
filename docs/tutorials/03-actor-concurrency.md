# Tutorial 3: Actor-Based Concurrency

**Time:** 2 hours  
**Prerequisites:** Tutorials 1 & 2  
**Goal:** Learn to write concurrent programs with actors

## What Are Actors?

Actors are independent units of computation that:
- Have their own **state** (private data)
- Process **messages** one at a time
- Communicate by **sending messages** (not sharing memory)
- Run **concurrently** without manual thread management

Think of actors like people sending letters to each other - they don't share notebooks, they send messages!

## Your First Actor

### Simple Counter Actor

```aether
actor Counter {
    state count = 0
    
    receive(msg) {
        if (msg.type == 1) {
            count = count + 1
        }
    }
}

main() {
    // Create a counter
    counter = spawn_Counter()
    
    // Send it messages
    send_Counter(counter, 1, 0)
    send_Counter(counter, 1, 0)
    send_Counter(counter, 1, 0)
    
    // Process the messages
    Counter_step(counter)
    Counter_step(counter)
    Counter_step(counter)
    
    print("Counter processed 3 messages!\n")
}
```

**What's happening:**
1. `spawn_Counter()` creates a new counter actor
2. `send_Counter(counter, 1, 0)` sends a message (type=1, payload=0)
3. `Counter_step(counter)` processes one message from the mailbox
4. Each message increments the count

## Actor Components

### 1. State Variables

State persists across messages:

```aether
actor BankAccount {
    state balance = 1000
    state account_number = 12345
    
    receive(msg) {
        // Can read and modify state
        if (msg.type == 1) {
            // Deposit
            balance = balance + msg.payload_int
        }
        if (msg.type == 2) {
            // Withdraw
            balance = balance - msg.payload_int
        }
    }
}
```

### 2. Message Types

Messages have three parts:
- `msg.type` - Integer message type (1, 2, 3, etc.)
- `msg.payload_int` - Integer data
- `msg.sender_id` - Who sent it

```aether
actor Processor {
    state total = 0
    
    receive(msg) {
        if (msg.type == 1) {
            // Add operation
            total = total + msg.payload_int
        }
        if (msg.type == 2) {
            // Multiply operation
            total = total * msg.payload_int
        }
        if (msg.type == 3) {
            // Reset operation
            total = 0
        }
    }
}

main() {
    proc = spawn_Processor()
    
    send_Processor(proc, 1, 10)  // Add 10
    send_Processor(proc, 1, 5)   // Add 5
    send_Processor(proc, 2, 2)   // Multiply by 2
    
    Processor_step(proc)  // total = 10
    Processor_step(proc)  // total = 15
    Processor_step(proc)  // total = 30
}
```

### 3. Message Processing

Messages are processed **one at a time** in the order received:

```aether
actor Logger {
    state message_count = 0
    
    receive(msg) {
        message_count = message_count + 1
        print("Message #")
        print(message_count)
        print(": type=")
        print(msg.type)
        print("\n")
    }
}

main() {
    logger = spawn_Logger()
    
    // Send 3 messages
    send_Logger(logger, 1, 0)
    send_Logger(logger, 2, 0)
    send_Logger(logger, 3, 0)
    
    // Process them one by one
    Logger_step(logger)  // Message #1: type=1
    Logger_step(logger)  // Message #2: type=2
    Logger_step(logger)  // Message #3: type=3
}
```

## Practical Examples

### Example 1: Shopping Cart

```aether
actor ShoppingCart {
    state item_count = 0
    state total_price = 0
    
    receive(msg) {
        if (msg.type == 1) {
            // Add item
            item_count = item_count + 1
            total_price = total_price + msg.payload_int
        }
        if (msg.type == 2) {
            // Remove item
            if (item_count > 0) {
                item_count = item_count - 1
                total_price = total_price - msg.payload_int
            }
        }
        if (msg.type == 3) {
            // Clear cart
            item_count = 0
            total_price = 0
        }
    }
}

main() {
    cart = spawn_ShoppingCart()
    
    // Add items (type=1, price)
    send_ShoppingCart(cart, 1, 100)  // $100 item
    send_ShoppingCart(cart, 1, 50)   // $50 item
    send_ShoppingCart(cart, 1, 75)   // $75 item
    
    ShoppingCart_step(cart)  // Process: 1 item, $100
    ShoppingCart_step(cart)  // Process: 2 items, $150
    ShoppingCart_step(cart)  // Process: 3 items, $225
    
    print("Shopping cart ready!\n")
}
```

### Example 2: Traffic Light

```aether
actor TrafficLight {
    state current_light = 1  // 1=red, 2=yellow, 3=green
    state duration = 0
    
    receive(msg) {
        if (msg.type == 1) {
            // Change light
            if (current_light == 1) {
                current_light = 3  // red -> green
            } else {
                if (current_light == 3) {
                    current_light = 2  // green -> yellow
                } else {
                    current_light = 1  // yellow -> red
                }
            }
        }
        if (msg.type == 2) {
            // Update duration
            duration = duration + 1
        }
    }
}

main() {
    light = spawn_TrafficLight()
    
    // Simulate time passing
    send_TrafficLight(light, 2, 0)  // tick
    send_TrafficLight(light, 1, 0)  // change
    send_TrafficLight(light, 2, 0)  // tick
    send_TrafficLight(light, 1, 0)  // change
    
    TrafficLight_step(light)
    TrafficLight_step(light)
    TrafficLight_step(light)
    TrafficLight_step(light)
}
```

### Example 3: Multiple Actors

```aether
actor Counter {
    state count = 0
    
    receive(msg) {
        count = count + 1
    }
}

main() {
    // Create multiple counters
    counter1 = spawn_Counter()
    counter2 = spawn_Counter()
    counter3 = spawn_Counter()
    
    // Send messages to different counters
    send_Counter(counter1, 1, 0)
    send_Counter(counter1, 1, 0)
    send_Counter(counter2, 1, 0)
    send_Counter(counter3, 1, 0)
    send_Counter(counter3, 1, 0)
    send_Counter(counter3, 1, 0)
    
    // Each processes independently
    Counter_step(counter1)
    Counter_step(counter1)
    Counter_step(counter2)
    Counter_step(counter3)
    Counter_step(counter3)
    Counter_step(counter3)
    
    print("Multiple actors running!\n")
}
```

## Actor Design Patterns

### Pattern 1: Request-Response

```aether
actor Calculator {
    state last_result = 0
    
    receive(msg) {
        if (msg.type == 1) {
            // Add operation
            last_result = msg.payload_int + 10
        }
        if (msg.type == 2) {
            // Multiply operation
            last_result = msg.payload_int * 2
        }
    }
}
```

### Pattern 2: State Machine

```aether
actor GamePlayer {
    state health = 100
    state alive = 1
    state level = 1
    
    receive(msg) {
        if (alive == 1) {
            if (msg.type == 1) {
                // Take damage
                health = health - msg.payload_int
                if (health <= 0) {
                    alive = 0
                }
            }
            if (msg.type == 2) {
                // Heal
                health = health + msg.payload_int
                if (health > 100) {
                    health = 100
                }
            }
            if (msg.type == 3) {
                // Level up
                level = level + 1
                health = 100
            }
        }
    }
}
```

### Pattern 3: Accumulator

```aether
actor Statistics {
    state total = 0
    state count = 0
    state max = 0
    
    receive(msg) {
        value = msg.payload_int
        
        total = total + value
        count = count + 1
        
        if (value > max) {
            max = value
        }
    }
}
```

## Exercises

### Exercise 1: Temperature Sensor

Create an actor that tracks temperature readings:

```aether
actor TempSensor {
    state current_temp = 20
    state readings = 0
    
    receive(msg) {
        // Type 1: Update temperature
        // Type 2: Count readings
        // Your code here
    }
}
```

### Exercise 2: Inventory Manager

Create an actor for game inventory:

```aether
actor Inventory {
    state gold = 0
    state items = 0
    
    receive(msg) {
        // Type 1: Add gold
        // Type 2: Spend gold
        // Type 3: Add item
        // Type 4: Remove item
        // Your code here
    }
}
```

### Exercise 3: Score Tracker

Create an actor that tracks game scores:

```aether
actor ScoreTracker {
    state score = 0
    state multiplier = 1
    state high_score = 0
    
    receive(msg) {
        // Type 1: Add points (use multiplier)
        // Type 2: Set multiplier
        // Type 3: Reset score
        // Update high_score if current score is higher
        // Your code here
    }
}
```

## Why Use Actors?

### Benefits

1. **No Race Conditions** - State is private, no shared memory
2. **Easy Concurrency** - No manual locks or mutexes
3. **Scalable** - Each actor is independent
4. **Fault Tolerant** - Actors can be supervised and restarted

### When to Use Actors

- ✅ Independent concurrent tasks
- ✅ State machines
- ✅ Event processing
- ✅ Game entities (players, NPCs, items)
- ✅ Microservices patterns

### When NOT to Use Actors

- ❌ Simple sequential code
- ❌ High-speed data sharing
- ❌ Tight coupling between components

## Best Practices

### 1. Keep State Small

```aether
// ✅ Good: Minimal state
actor Counter {
    state count = 0
}

// ❌ Avoid: Too much state
actor Everything {
    state count = 0
    state name = ""
    state data1 = 0
    state data2 = 0
    // ... 20 more fields
}
```

### 2. Use Clear Message Types

```aether
// Define message types as constants (conceptually)
// Type 1 = INCREMENT
// Type 2 = DECREMENT
// Type 3 = RESET

actor Counter {
    state count = 0
    
    receive(msg) {
        if (msg.type == 1) {  // INCREMENT
            count = count + 1
        }
        if (msg.type == 2) {  // DECREMENT
            count = count - 1
        }
        if (msg.type == 3) {  // RESET
            count = 0
        }
    }
}
```

### 3. Process All Messages

```aether
// Make sure to call _step() for each message
send_Counter(c, 1, 0)
send_Counter(c, 1, 0)
send_Counter(c, 1, 0)

Counter_step(c)  // Process message 1
Counter_step(c)  // Process message 2
Counter_step(c)  // Process message 3
```

## Next Steps

You've learned:
- ✅ What actors are
- ✅ Actor state and messages
- ✅ Spawning and messaging
- ✅ Multiple concurrent actors
- ✅ Actor design patterns

**Next Tutorial:** [Advanced Topics](04-advanced-topics.md)

## Quick Reference

```aether
// Define actor
actor MyActor {
    state value = 0
    
    receive(msg) {
        if (msg.type == 1) {
            // Handle message type 1
        }
    }
}

// Use actor
actor = spawn_MyActor()
send_MyActor(actor, 1, 42)
MyActor_step(actor)

// Message fields
msg.type         // Message type (int)
msg.payload_int  // Integer payload
msg.sender_id    // Sender ID
```

Happy concurrent programming!

