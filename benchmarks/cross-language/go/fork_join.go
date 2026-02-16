// Go Fork-Join Benchmark (Savina-style)
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

const numWorkers = 8

var messagesPerWorker = getMessages() / numWorkers

func worker(id int, inbox chan int, wg *sync.WaitGroup, result *int) {
	defer wg.Done()
	count := 0
	for range inbox {
		count++
	}
	*result = count
}

func main() {
	total := numWorkers * messagesPerWorker
	fmt.Println("=== Go Fork-Join Throughput Benchmark ===")
	fmt.Printf("Workers: %d, Messages: %d\n\n", numWorkers, total)

	// Create worker channels
	channels := make([]chan int, numWorkers)
	results := make([]int, numWorkers)
	var wg sync.WaitGroup

	for i := 0; i < numWorkers; i++ {
		channels[i] = make(chan int, 1024)
		wg.Add(1)
		go worker(i, channels[i], &wg, &results[i])
	}

	start := time.Now()

	// Send messages round-robin
	for i := 0; i < total; i++ {
		channels[i%numWorkers] <- i
	}

	// Close all channels
	for i := 0; i < numWorkers; i++ {
		close(channels[i])
	}

	// Wait for all workers
	wg.Wait()

	elapsed := time.Since(start)

	// Collect results
	totalProcessed := 0
	for i := 0; i < numWorkers; i++ {
		totalProcessed += results[i]
	}

	if totalProcessed != total {
		fmt.Printf("VALIDATION FAILED: expected %d, got %d\n", total, totalProcessed)
	}

	elapsedSec := elapsed.Seconds()
	throughput := float64(total) / elapsedSec / 1e6
	cyclesPerMsg := elapsedSec * 3e9 / float64(total)

	fmt.Printf("Cycles/msg:     %.2f\n", cyclesPerMsg)
	fmt.Printf("Throughput:     %.2f M msg/sec\n", throughput)
}
