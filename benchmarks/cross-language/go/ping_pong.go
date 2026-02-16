// Go Ping-Pong Benchmark (Savina-style)
// Uses buffered channels to model actor mailboxes (actors have queues, not CSP rendezvous)
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
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

var MESSAGES = getMessages()

func main() {
	fmt.Println("=== Go Ping-Pong Benchmark ===")
	fmt.Printf("Messages: %d\n", MESSAGES)
	fmt.Println("Using goroutines with buffered channels (actor mailbox model)")
	fmt.Println()

	// Buffered channels model actor mailboxes
	// Actors have queues, not CSP-style synchronous rendezvous
	// Size 1 ensures strict request-response ordering while allowing async sends
	chanA := make(chan int, 1)
	chanB := make(chan int, 1)

	var wg sync.WaitGroup
	wg.Add(2)

	start := time.Now()

	// Ping goroutine
	go func() {
		defer wg.Done()
		for i := 0; i < MESSAGES; i++ {
			chanA <- i
			received := <-chanB
			if received != i {
				fmt.Fprintf(os.Stderr, "ERROR: Ping sent %d but got back %d\n", i, received)
			}
		}
	}()

	// Pong goroutine
	go func() {
		defer wg.Done()
		for i := 0; i < MESSAGES; i++ {
			received := <-chanA
			if received != i {
				fmt.Fprintf(os.Stderr, "ERROR: Pong expected %d but got %d\n", i, received)
			}
			chanB <- received
		}
	}()

	wg.Wait()
	elapsed := time.Since(start)

	totalNs := float64(elapsed.Nanoseconds())
	nsPerMsg := totalNs / float64(MESSAGES)
	throughput := 1e9 / nsPerMsg
	cyclesPerMsg := nsPerMsg * 3.0

	fmt.Printf("Cycles/msg:     %.2f\n", cyclesPerMsg)
	fmt.Printf("Throughput:     %.2f M msg/sec\n", throughput/1e6)
}
