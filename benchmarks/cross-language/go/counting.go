// Go Counting Actor Benchmark (Savina-style)
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

var messages = getMessages()

func counter(inbox chan struct{}, done chan int) {
	count := 0
	for range inbox {
		count++
	}
	done <- count
}

func main() {
	fmt.Println("=== Go Counting Actor Benchmark ===")
	fmt.Printf("Messages: %d\n\n", messages)

	inbox := make(chan struct{}, 1024)
	done := make(chan int)

	go counter(inbox, done)

	start := time.Now()

	// Send all messages
	for i := 0; i < messages; i++ {
		inbox <- struct{}{}
	}
	close(inbox)

	// Wait for count
	count := <-done

	elapsed := time.Since(start)

	if count != messages {
		fmt.Printf("VALIDATION FAILED: expected %d, got %d\n", messages, count)
	}

	elapsedSec := elapsed.Seconds()
	throughput := float64(messages) / elapsedSec / 1e6
	cyclesPerMsg := elapsedSec * 3e9 / float64(messages)

	fmt.Printf("Cycles/msg:     %.2f\n", cyclesPerMsg)
	fmt.Printf("Throughput:     %.2f M msg/sec\n", throughput)
}
