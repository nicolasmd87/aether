// CUDA GPU Acceleration Benchmark
// Requires: NVIDIA GPU + CUDA Toolkit

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cuda_runtime.h>

// Error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err)); \
            exit(1); \
        } \
    } while (0)

// Actor state (on GPU)
typedef struct {
    int counter;
    int mailbox_head;
    int mailbox_tail;
    int mailbox_count;
} Actor;

// Message
typedef struct {
    int type;
    int payload;
} Message;

// CUDA Kernel: Process actors in parallel
__global__ void actor_step_kernel(
    Actor* actors,
    Message* messages,
    int actor_count,
    int messages_per_actor
) {
    int actor_id = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (actor_id < actor_count) {
        Actor* actor = &actors[actor_id];
        
        // Process all messages for this actor
        for (int m = 0; m < messages_per_actor; m++) {
            int msg_idx = actor_id * messages_per_actor + m;
            Message msg = messages[msg_idx];
            actor->counter += msg.payload;
        }
    }
}

// CPU baseline for comparison
void process_cpu(Actor* actors, Message* messages, int actor_count, int messages_per_actor) {
    for (int i = 0; i < actor_count; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            int msg_idx = i * messages_per_actor + m;
            actors[i].counter += messages[msg_idx].payload;
        }
    }
}

int main(int argc, char* argv[]) {
    int num_actors = 100000;
    int messages_per_actor = 10;
    
    if (argc >= 2) num_actors = atoi(argv[1]);
    if (argc >= 3) messages_per_actor = atoi(argv[2]);
    
    printf("=== GPU Acceleration Benchmark (CUDA) ===\n");
    
    // Check for CUDA device
    int device_count = 0;
    CUDA_CHECK(cudaGetDeviceCount(&device_count));
    
    if (device_count == 0) {
        printf("ERROR: No CUDA devices found!\n");
        return 1;
    }
    
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    printf("GPU: %s\n", prop.name);
    printf("CUDA Cores: %d\n", prop.multiProcessorCount * 128);  // Approximate
    printf("Memory: %.2f GB\n\n", prop.totalGlobalMem / 1e9);
    
    printf("Actors: %d\n", num_actors);
    printf("Messages per actor: %d\n", messages_per_actor);
    
    long long total_messages = (long long)num_actors * messages_per_actor;
    printf("Total messages: %lld\n\n", total_messages);
    
    // Allocate host memory
    size_t actor_size = sizeof(Actor) * num_actors;
    size_t message_size = sizeof(Message) * total_messages;
    
    Actor* h_actors = (Actor*)malloc(actor_size);
    Message* h_messages = (Message*)malloc(message_size);
    
    // Initialize data
    for (int i = 0; i < num_actors; i++) {
        h_actors[i].counter = 0;
        h_actors[i].mailbox_head = 0;
        h_actors[i].mailbox_tail = 0;
        h_actors[i].mailbox_count = 0;
    }
    
    for (long long i = 0; i < total_messages; i++) {
        h_messages[i].type = 1;
        h_messages[i].payload = 1;
    }
    
    // Test 1: CPU Baseline
    printf("Test 1: CPU Baseline\n");
    
    // Create copy for CPU test
    Actor* cpu_actors = (Actor*)malloc(actor_size);
    memcpy(cpu_actors, h_actors, actor_size);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    process_cpu(cpu_actors, h_messages, num_actors, messages_per_actor);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_cpu = (end.tv_sec - start.tv_sec) + 
                      (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_cpu = total_messages / time_cpu / 1e6;
    
    printf("  Time:       %.4f seconds\n", time_cpu);
    printf("  Throughput: %.2f M msg/sec\n\n", throughput_cpu);
    
    // Verify
    long long cpu_sum = 0;
    for (int i = 0; i < num_actors; i++) {
        cpu_sum += cpu_actors[i].counter;
    }
    free(cpu_actors);
    
    // Test 2: GPU with Transfer
    printf("Test 2: GPU (including transfer time)\n");
    
    Actor* d_actors;
    Message* d_messages;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Allocate GPU memory
    CUDA_CHECK(cudaMalloc(&d_actors, actor_size));
    CUDA_CHECK(cudaMalloc(&d_messages, message_size));
    
    // Transfer to GPU
    CUDA_CHECK(cudaMemcpy(d_actors, h_actors, actor_size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_messages, h_messages, message_size, cudaMemcpyHostToDevice));
    
    // Launch kernel
    int threads_per_block = 256;
    int blocks = (num_actors + threads_per_block - 1) / threads_per_block;
    
    actor_step_kernel<<<blocks, threads_per_block>>>(
        d_actors, d_messages, num_actors, messages_per_actor
    );
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    // Transfer back
    CUDA_CHECK(cudaMemcpy(h_actors, d_actors, actor_size, cudaMemcpyDeviceToHost));
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_gpu_total = (end.tv_sec - start.tv_sec) + 
                            (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_gpu_total = total_messages / time_gpu_total / 1e6;
    double speedup_total = time_cpu / time_gpu_total;
    
    printf("  Time:       %.4f seconds\n", time_gpu_total);
    printf("  Throughput: %.2f M msg/sec\n", throughput_gpu_total);
    printf("  Speedup:    %.2fx vs CPU\n\n", speedup_total);
    
    // Verify
    long long gpu_sum = 0;
    for (int i = 0; i < num_actors; i++) {
        gpu_sum += h_actors[i].counter;
    }
    
    if (gpu_sum != cpu_sum) {
        printf("  ERROR: GPU result (%lld) != CPU result (%lld)\n", gpu_sum, cpu_sum);
    } else {
        printf("  Status:     PASS (verified)\n\n");
    }
    
    // Test 3: GPU Kernel Only (no transfer)
    printf("Test 3: GPU Kernel Only (data already on GPU)\n");
    
    // Reset counters on GPU
    CUDA_CHECK(cudaMemcpy(d_actors, h_actors, actor_size, cudaMemcpyHostToDevice));
    for (int i = 0; i < num_actors; i++) {
        h_actors[i].counter = 0;
    }
    CUDA_CHECK(cudaMemcpy(d_actors, h_actors, actor_size, cudaMemcpyHostToDevice));
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    actor_step_kernel<<<blocks, threads_per_block>>>(
        d_actors, d_messages, num_actors, messages_per_actor
    );
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_gpu_kernel = (end.tv_sec - start.tv_sec) + 
                             (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_gpu_kernel = total_messages / time_gpu_kernel / 1e6;
    double speedup_kernel = time_cpu / time_gpu_kernel;
    
    printf("  Time:       %.4f seconds\n", time_gpu_kernel);
    printf("  Throughput: %.2f M msg/sec\n", throughput_gpu_kernel);
    printf("  Speedup:    %.2fx vs CPU\n\n", speedup_kernel);
    
    // Cleanup
    CUDA_CHECK(cudaFree(d_actors));
    CUDA_CHECK(cudaFree(d_messages));
    free(h_actors);
    free(h_messages);
    
    // Summary
    printf("=== Summary ===\n");
    printf("CPU:                %.2f M msg/sec\n", throughput_cpu);
    printf("GPU (with transfer): %.2f M msg/sec (%.2fx)\n", throughput_gpu_total, speedup_total);
    printf("GPU (kernel only):  %.2f M msg/sec (%.2fx)\n", throughput_gpu_kernel, speedup_kernel);
    printf("\n");
    
    if (speedup_kernel > 5.0) {
        printf("Result: GPU acceleration HIGHLY BENEFICIAL for this workload!\n");
    } else if (speedup_total > 1.5) {
        printf("Result: GPU acceleration beneficial for persistent actors.\n");
    } else {
        printf("Result: CPU faster due to PCIe transfer overhead.\n");
    }
    
    return 0;
}
