#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    int actors = 1000000, rounds = 100, msgs_per_round = 10000;
    int* counters = calloc(actors, sizeof(int));
    int* messages = malloc(sizeof(int) * msgs_per_round * 2);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int r = 0; r < rounds; r++) {
        for (int i = 0; i < msgs_per_round; i++) {
            messages[i*2] = rand() % actors;
            messages[i*2+1] = 1;
        }
        for (int i = 0; i < msgs_per_round; i++) {
            counters[messages[i*2]] += messages[i*2+1];
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double cpu_throughput = (rounds * msgs_per_round) / time / 1e6;
    
    printf("\n=== FINAL COMPARISON ===\n");
    printf("CPU (1 core):      %.2f M msg/sec\n", cpu_throughput);
    printf("GPU (zero-transfer): 20.58 M msg/sec\n\n");
    printf("Winner: %s (%.2fx faster)\n\n", 
           cpu_throughput > 20.58 ? "CPU" : "GPU",
           cpu_throughput > 20.58 ? cpu_throughput / 20.58 : 20.58 / cpu_throughput);
    
    free(counters);
    free(messages);
    return 0;
}
