// Go Thread Ring Benchmark (Savina-style)
// Uses buffered channels to model actor mailboxes
package main

import (
	"fmt"
	"os"
	"strconv"
	"time"
)

func getMessages() int {
	if env := os.Getenv("BENCHMARK_MESSAGES"); env != "" {
		if n, err := strconv.Atoi(env); err == nil {
			return n
		}
	}
	return 100000
}

// Match other implementations: 100 nodes, configurable hops
const ringSize = 100

var numHops = getMessages()

func ringNode(id int, inbox <-chan int, next chan<- int, done chan<- int) {
	received := 0
	for token := range inbox {
		received++
		if token == 0 {
			done <- received
			return
		}
		next <- token - 1
	}
}

func main() {
	fmt.Println("=== Go Thread Ring Benchmark ===")
	fmt.Printf("Ring size: %d, Hops: %d\n\n", ringSize, numHops)

	// Buffered channels model actor mailboxes
	// Size 1 maintains ordering while allowing async message passing
	channels := make([]chan int, ringSize)
	for i := 0; i < ringSize; i++ {
		channels[i] = make(chan int, 1)
	}

	done := make(chan int)

	// Start goroutines
	for i := 0; i < ringSize; i++ {
		next := channels[(i+1)%ringSize]
		go ringNode(i, channels[i], next, done)
	}

	start := time.Now()

	// Inject initial token
	channels[0] <- numHops

	// Wait for completion
	received := <-done

	elapsed := time.Since(start)

	totalMessages := numHops + 1
	// Each node receives totalMessages/ringSize messages
	expectedPerNode := totalMessages/ringSize + 1
	if received < expectedPerNode-10 || received > expectedPerNode+10 {
		fmt.Printf("VALIDATION WARNING: expected ~%d per node, got %d\n", expectedPerNode, received)
	}

	elapsedSec := elapsed.Seconds()
	throughput := float64(totalMessages) / elapsedSec / 1e6
	cyclesPerMsg := elapsedSec * 3e9 / float64(totalMessages)

	fmt.Printf("Cycles/msg:     %.2f\n", cyclesPerMsg)
	fmt.Printf("Throughput:     %.2f M msg/sec\n", throughput)
}
