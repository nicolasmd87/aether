// Zero-Transfer GPU: Actors live on GPU, messages streamed
// Strategy: Eliminate actor transfer overhead by keeping them GPU-resident

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#else
#include <CL/cl.h>
#endif

// Efficient kernel: Each thread processes ONE message
const char* kernel_source = 
"__kernel void process_messages(\n"
"    __global int* counters,\n"
"    __global int* incoming_messages,\n"
"    int message_count\n"
") {\n"
"    int msg_id = get_global_id(0);\n"
"    if (msg_id < message_count) {\n"
"        int target = incoming_messages[msg_id * 2];\n"
"        int payload = incoming_messages[msg_id * 2 + 1];\n"
"        atomic_add(&counters[target], payload);\n"
"    }\n"
"}\n";

void check_cl_error(cl_int err, const char* msg) {
    if (err != CL_SUCCESS) {
        fprintf(stderr, "OpenCL Error %d: %s\n", err, msg);
        exit(1);
    }
}

int main() {
    printf("=== Zero-Transfer GPU: Stream Messages Only ===\n\n");
    
    int num_actors = 1000000;
    int rounds = 100;
    int messages_per_round = 10000;
    
    printf("Actors: %d (GPU-resident)\n", num_actors);
    printf("Rounds: %d\n", rounds);
    printf("Messages per round: %d\n", messages_per_round);
    printf("Total messages: %lld\n\n", (long long)rounds * messages_per_round);
    
    // Setup OpenCL
    cl_platform_id platform;
    cl_device_id device;
    cl_int err = clGetPlatformIDs(1, &platform, NULL);
    check_cl_error(err, "Get platform");
    
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    check_cl_error(err, "Get device");
    
    char device_name[128];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    printf("Device: %s\n\n", device_name);
    
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_cl_error(err, "Create context");
    
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
    check_cl_error(err, "Create queue");
    
    cl_program program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    check_cl_error(err, "Create program");
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    check_cl_error(err, "Build program");
    
    cl_kernel kernel = clCreateKernel(program, "process_messages", &err);
    check_cl_error(err, "Create kernel");
    
    // ========================================
    // Test 1: Traditional (transfer actors each round)
    // ========================================
    printf("Test 1: Traditional (transfer actors each round)\n");
    
    int* counters = (int*)calloc(num_actors, sizeof(int));
    int* messages = (int*)malloc(sizeof(int) * messages_per_round * 2); // [target, payload] pairs
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    cl_mem d_counters_trad = clCreateBuffer(context, CL_MEM_READ_WRITE, 
                                            sizeof(int) * num_actors, NULL, &err);
    cl_mem d_messages_trad = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                            sizeof(int) * messages_per_round * 2, NULL, &err);
    
    for (int round = 0; round < rounds; round++) {
        // Generate messages
        for (int i = 0; i < messages_per_round; i++) {
            messages[i * 2] = rand() % num_actors;  // target
            messages[i * 2 + 1] = 1;                 // payload
        }
        
        // Transfer counters TO GPU
        clEnqueueWriteBuffer(queue, d_counters_trad, CL_TRUE, 0,
                            sizeof(int) * num_actors, counters, 0, NULL, NULL);
        
        // Transfer messages TO GPU
        clEnqueueWriteBuffer(queue, d_messages_trad, CL_TRUE, 0,
                            sizeof(int) * messages_per_round * 2, messages, 0, NULL, NULL);
        
        // Execute
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_counters_trad);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_messages_trad);
        clSetKernelArg(kernel, 2, sizeof(int), &messages_per_round);
        
        size_t global_size = messages_per_round;  // One thread per MESSAGE
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
        clFinish(queue);
        
        // Transfer counters BACK
        clEnqueueReadBuffer(queue, d_counters_trad, CL_TRUE, 0,
                           sizeof(int) * num_actors, counters, 0, NULL, NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_trad = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_trad = ((long long)rounds * messages_per_round) / time_trad / 1e6;
    
    printf("  Time:       %.4f seconds\n", time_trad);
    printf("  Throughput: %.2f M msg/sec\n\n", throughput_trad);
    
    long long sum_trad = 0;
    for (int i = 0; i < num_actors; i++) sum_trad += counters[i];
    
    clReleaseMemObject(d_counters_trad);
    clReleaseMemObject(d_messages_trad);
    
    // ========================================
    // Test 2: Zero-Transfer (actors stay on GPU)
    // ========================================
    printf("Test 2: Zero-Transfer (actors GPU-resident, stream messages only)\n");
    
    memset(counters, 0, sizeof(int) * num_actors);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Create PERSISTENT actor buffer on GPU
    cl_mem d_counters_persist = clCreateBuffer(context, CL_MEM_READ_WRITE,
                                               sizeof(int) * num_actors, NULL, &err);
    
    // Initialize actors on GPU ONCE
    clEnqueueWriteBuffer(queue, d_counters_persist, CL_TRUE, 0,
                        sizeof(int) * num_actors, counters, 0, NULL, NULL);
    
    // Create message stream buffer
    cl_mem d_messages_stream = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                              sizeof(int) * messages_per_round * 2, NULL, &err);
    
    for (int round = 0; round < rounds; round++) {
        // Generate messages
        for (int i = 0; i < messages_per_round; i++) {
            messages[i * 2] = rand() % num_actors;
            messages[i * 2 + 1] = 1;
        }
        
        // ONLY transfer new messages (actors stay on GPU!)
        clEnqueueWriteBuffer(queue, d_messages_stream, CL_FALSE, 0,
                            sizeof(int) * messages_per_round * 2, messages, 0, NULL, NULL);
        
        // Execute
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_counters_persist);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_messages_stream);
        clSetKernelArg(kernel, 2, sizeof(int), &messages_per_round);
        
        size_t global_size = num_actors;
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
    }
    
    // Only read back at the END
    clFinish(queue);
    clEnqueueReadBuffer(queue, d_counters_persist, CL_TRUE, 0,
                       sizeof(int) * num_actors, counters, 0, NULL, NULL);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_zero = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_zero = ((long long)rounds * messages_per_round) / time_zero / 1e6;
    double speedup = time_trad / time_zero;
    
    printf("  Time:       %.4f seconds\n", time_zero);
    printf("  Throughput: %.2f M msg/sec\n", throughput_zero);
    printf("  Speedup:    %.2fx vs traditional\n\n", speedup);
    
    long long sum_zero = 0;
    for (int i = 0; i < num_actors; i++) sum_zero += counters[i];
    
    if (sum_zero != sum_trad) {
        printf("  ERROR: Zero-transfer (%lld) != Traditional (%lld)\n", sum_zero, sum_trad);
    } else {
        printf("  Status:     PASS (verified)\n\n");
    }
    
    // ========================================
    // Test 3: Pinned Memory (DMA-friendly)
    // ========================================
    printf("Test 3: Pinned Memory (zero-copy host memory)\n");
    
    // Allocate pinned (page-locked) host memory
    int* pinned_counters = (int*)clEnqueueMapBuffer(queue, d_counters_persist,
                                                     CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                                                     0, sizeof(int) * num_actors,
                                                     0, NULL, NULL, &err);
    check_cl_error(err, "Map buffer");
    
    memset(pinned_counters, 0, sizeof(int) * num_actors);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int round = 0; round < rounds; round++) {
        for (int i = 0; i < messages_per_round; i++) {
            messages[i * 2] = rand() % num_actors;
            messages[i * 2 + 1] = 1;
        }
        
        clEnqueueWriteBuffer(queue, d_messages_stream, CL_FALSE, 0,
                            sizeof(int) * messages_per_round * 2, messages, 0, NULL, NULL);
        
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_counters_persist);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_messages_stream);
        clSetKernelArg(kernel, 2, sizeof(int), &messages_per_round);
        
        size_t global_size = messages_per_round;  // One thread per MESSAGE
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
    }
    
    clFinish(queue);
    
    // Pinned memory already accessible (no transfer!)
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_pinned = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_pinned = ((long long)rounds * messages_per_round) / time_pinned / 1e6;
    double speedup_pinned = time_trad / time_pinned;
    
    printf("  Time:       %.4f seconds\n", time_pinned);
    printf("  Throughput: %.2f M msg/sec\n", throughput_pinned);
    printf("  Speedup:    %.2fx vs traditional\n\n", speedup_pinned);
    
    long long sum_pinned = 0;
    for (int i = 0; i < num_actors; i++) sum_pinned += pinned_counters[i];
    printf("  Status:     %s (sum=%lld)\n\n", sum_pinned == sum_trad ? "PASS" : "FAIL", sum_pinned);
    
    // Summary
    printf("=== Summary ===\n");
    printf("Traditional (transfer actors every round):  %.2f M msg/sec\n", throughput_trad);
    printf("Zero-Transfer (actors stay on GPU):         %.2f M msg/sec (%.2fx)\n", 
           throughput_zero, speedup);
    printf("Pinned Memory (zero-copy):                  %.2f M msg/sec (%.2fx)\n\n",
           throughput_pinned, speedup_pinned);
    
    if (speedup > 1.5) {
        printf("Result: BREAKTHROUGH! Zero-transfer gives %.2fx speedup\n", speedup);
        printf("Key insight: Keep actors on GPU, only stream messages\n");
    } else {
        printf("Result: Even with optimizations, CPU is still competitive\n");
        printf("Reason: Message streaming still requires PCIe bandwidth\n");
    }
    
    // Cleanup
    clEnqueueUnmapMemObject(queue, d_counters_persist, pinned_counters, 0, NULL, NULL);
    clReleaseMemObject(d_counters_persist);
    clReleaseMemObject(d_messages_stream);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    
    free(counters);
    free(messages);
    
    return 0;
}
