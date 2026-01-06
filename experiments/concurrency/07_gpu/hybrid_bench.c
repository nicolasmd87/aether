// Hybrid CPU+GPU Benchmark - GPU as overflow handler
// Strategy: CPU processes normal load, GPU takes overflow when CPU saturated

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#else
#include <CL/cl.h>
#endif

// OpenCL kernel
const char* kernel_source = 
"__kernel void actor_step_kernel(\n"
"    __global int* counters,\n"
"    __global int* payloads,\n"
"    int messages_per_actor,\n"
"    int offset\n"
") {\n"
"    int actor_id = get_global_id(0) + offset;\n"
"    int local_id = get_global_id(0);\n"
"    \n"
"    for (int m = 0; m < messages_per_actor; m++) {\n"
"        counters[local_id] += payloads[local_id * messages_per_actor + m];\n"
"    }\n"
"}\n";

typedef struct {
    int* counters;
    int* payloads;
    int start_actor;
    int end_actor;
    int messages_per_actor;
    double time_taken;
} CPUWorker;

void* cpu_worker_thread(void* arg) {
    CPUWorker* worker = (CPUWorker*)arg;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = worker->start_actor; i < worker->end_actor; i++) {
        for (int m = 0; m < worker->messages_per_actor; m++) {
            worker->counters[i] += worker->payloads[i * worker->messages_per_actor + m];
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    worker->time_taken = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
    
    return NULL;
}

void check_cl_error(cl_int err, const char* msg) {
    if (err != CL_SUCCESS) {
        fprintf(stderr, "OpenCL Error %d: %s\n", err, msg);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    int total_actors = 1000000;
    int messages_per_actor = 100;
    int cpu_cores = 8;
    
    if (argc >= 2) total_actors = atoi(argv[1]);
    if (argc >= 3) messages_per_actor = atoi(argv[2]);
    if (argc >= 4) cpu_cores = atoi(argv[3]);
    
    printf("=== Hybrid CPU+GPU Benchmark ===\n");
    printf("Total actors: %d\n", total_actors);
    printf("Messages per actor: %d\n", messages_per_actor);
    printf("CPU cores: %d\n\n", cpu_cores);
    
    long long total_messages = (long long)total_actors * messages_per_actor;
    
    // Initialize data
    int* counters = (int*)calloc(total_actors, sizeof(int));
    int* payloads = (int*)malloc(sizeof(int) * total_messages);
    
    for (long long i = 0; i < total_messages; i++) {
        payloads[i] = 1;
    }
    
    // ========================================
    // Test 1: CPU Only (Baseline)
    // ========================================
    printf("Test 1: CPU Only (%d cores)\n", cpu_cores);
    
    int* cpu_only_counters = (int*)calloc(total_actors, sizeof(int));
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * cpu_cores);
    CPUWorker* workers = (CPUWorker*)malloc(sizeof(CPUWorker) * cpu_cores);
    
    int actors_per_core = total_actors / cpu_cores;
    
    for (int i = 0; i < cpu_cores; i++) {
        workers[i].counters = cpu_only_counters;
        workers[i].payloads = payloads;
        workers[i].start_actor = i * actors_per_core;
        workers[i].end_actor = (i == cpu_cores - 1) ? total_actors : (i + 1) * actors_per_core;
        workers[i].messages_per_actor = messages_per_actor;
        pthread_create(&threads[i], NULL, cpu_worker_thread, &workers[i]);
    }
    
    for (int i = 0; i < cpu_cores; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_cpu_only = (end.tv_sec - start.tv_sec) + 
                           (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_cpu_only = total_messages / time_cpu_only / 1e6;
    
    printf("  Time:       %.4f seconds\n", time_cpu_only);
    printf("  Throughput: %.2f M msg/sec\n\n", throughput_cpu_only);
    
    long long cpu_only_sum = 0;
    for (int i = 0; i < total_actors; i++) {
        cpu_only_sum += cpu_only_counters[i];
    }
    
    // ========================================
    // Test 2: Hybrid (CPU + GPU overflow)
    // ========================================
    printf("Test 2: Hybrid (CPU handles %d%%, GPU handles overflow)\n", 
           (cpu_cores * 100) / (cpu_cores + 1));
    
    // Initialize OpenCL
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    
    cl_int err = clGetPlatformIDs(1, &platform, NULL);
    if (err != CL_SUCCESS) {
        printf("  ERROR: No OpenCL platform found\n");
        printf("  Skipping hybrid test...\n\n");
        goto cleanup;
    }
    
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) {
        printf("  ERROR: No GPU found\n");
        printf("  Skipping hybrid test...\n\n");
        goto cleanup;
    }
    
    char device_name[128];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    printf("  GPU: %s\n", device_name);
    
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_cl_error(err, "Create context");
    
    queue = clCreateCommandQueue(context, device, 0, &err);
    check_cl_error(err, "Create queue");
    
    program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    check_cl_error(err, "Create program");
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    check_cl_error(err, "Build program");
    
    kernel = clCreateKernel(program, "actor_step_kernel", &err);
    check_cl_error(err, "Create kernel");
    
    // Strategy: CPU handles N cores worth, GPU handles 1 core's worth
    int cpu_actors = (total_actors * cpu_cores) / (cpu_cores + 1);
    int gpu_actors = total_actors - cpu_actors;
    
    printf("  CPU actors: %d\n", cpu_actors);
    printf("  GPU actors: %d (overflow)\n\n", gpu_actors);
    
    int* hybrid_counters = (int*)calloc(total_actors, sizeof(int));
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Start CPU workers (parallel)
    for (int i = 0; i < cpu_cores; i++) {
        workers[i].counters = hybrid_counters;
        workers[i].payloads = payloads;
        workers[i].start_actor = i * (cpu_actors / cpu_cores);
        workers[i].end_actor = (i == cpu_cores - 1) ? cpu_actors : (i + 1) * (cpu_actors / cpu_cores);
        workers[i].messages_per_actor = messages_per_actor;
        pthread_create(&threads[i], NULL, cpu_worker_thread, &workers[i]);
    }
    
    // Start GPU worker (parallel with CPU)
    cl_mem d_counters = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
                                       sizeof(int) * gpu_actors, NULL, &err);
    check_cl_error(err, "Create counter buffer");
    
    cl_mem d_payloads = clCreateBuffer(context, CL_MEM_READ_ONLY, 
                                       sizeof(int) * gpu_actors * messages_per_actor, NULL, &err);
    check_cl_error(err, "Create payload buffer");
    
    // Transfer GPU data
    int* gpu_counters_temp = (int*)calloc(gpu_actors, sizeof(int));
    err = clEnqueueWriteBuffer(queue, d_counters, CL_FALSE, 0, 
                               sizeof(int) * gpu_actors, gpu_counters_temp, 0, NULL, NULL);
    
    err = clEnqueueWriteBuffer(queue, d_payloads, CL_FALSE, 0, 
                               sizeof(int) * gpu_actors * messages_per_actor, 
                               &payloads[cpu_actors * messages_per_actor], 0, NULL, NULL);
    
    // Launch GPU kernel
    int offset = cpu_actors;
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_counters);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_payloads);
    err |= clSetKernelArg(kernel, 2, sizeof(int), &messages_per_actor);
    err |= clSetKernelArg(kernel, 3, sizeof(int), &offset);
    check_cl_error(err, "Set kernel args");
    
    size_t global_size = gpu_actors;
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
    check_cl_error(err, "Execute kernel");
    
    // Wait for CPU threads
    for (int i = 0; i < cpu_cores; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Wait for GPU
    clFinish(queue);
    
    // Read GPU results
    err = clEnqueueReadBuffer(queue, d_counters, CL_TRUE, 0, 
                              sizeof(int) * gpu_actors, 
                              &hybrid_counters[cpu_actors], 0, NULL, NULL);
    check_cl_error(err, "Read results");
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_hybrid = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_hybrid = total_messages / time_hybrid / 1e6;
    double speedup = time_cpu_only / time_hybrid;
    
    printf("  Time:       %.4f seconds\n", time_hybrid);
    printf("  Throughput: %.2f M msg/sec\n", throughput_hybrid);
    printf("  Speedup:    %.2fx vs CPU-only\n\n", speedup);
    
    // Verify
    long long hybrid_sum = 0;
    for (int i = 0; i < total_actors; i++) {
        hybrid_sum += hybrid_counters[i];
    }
    
    if (hybrid_sum != cpu_only_sum) {
        printf("  ERROR: Hybrid (%lld) != CPU-only (%lld)\n", hybrid_sum, cpu_only_sum);
    } else {
        printf("  Status:     PASS (verified)\n\n");
    }
    
    // ========================================
    // Test 3: Extreme Load (2x actors)
    // ========================================
    printf("Test 3: Extreme Load (2× actors to saturate CPU)\n");
    
    int extreme_actors = total_actors * 2;
    long long extreme_messages = (long long)extreme_actors * messages_per_actor;
    
    int* extreme_counters_cpu = (int*)calloc(extreme_actors, sizeof(int));
    int* extreme_counters_hybrid = (int*)calloc(extreme_actors, sizeof(int));
    int* extreme_payloads = (int*)malloc(sizeof(int) * extreme_messages);
    
    for (long long i = 0; i < extreme_messages; i++) {
        extreme_payloads[i] = 1;
    }
    
    printf("  Testing %d actors (%lld messages)...\n\n", extreme_actors, extreme_messages);
    
    // CPU-only under extreme load
    printf("  3a. CPU-only:\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int extreme_actors_per_core = extreme_actors / cpu_cores;
    for (int i = 0; i < cpu_cores; i++) {
        workers[i].counters = extreme_counters_cpu;
        workers[i].payloads = extreme_payloads;
        workers[i].start_actor = i * extreme_actors_per_core;
        workers[i].end_actor = (i == cpu_cores - 1) ? extreme_actors : (i + 1) * extreme_actors_per_core;
        workers[i].messages_per_actor = messages_per_actor;
        pthread_create(&threads[i], NULL, cpu_worker_thread, &workers[i]);
    }
    
    for (int i = 0; i < cpu_cores; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_extreme_cpu = (end.tv_sec - start.tv_sec) + 
                              (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_extreme_cpu = extreme_messages / time_extreme_cpu / 1e6;
    
    printf("    Time:       %.4f seconds\n", time_extreme_cpu);
    printf("    Throughput: %.2f M msg/sec\n\n", throughput_extreme_cpu);
    
    // Hybrid under extreme load
    printf("  3b. Hybrid (CPU + GPU overflow):\n");
    
    int extreme_cpu_actors = (extreme_actors * cpu_cores) / (cpu_cores + 1);
    int extreme_gpu_actors = extreme_actors - extreme_cpu_actors;
    
    printf("    CPU: %d actors\n", extreme_cpu_actors);
    printf("    GPU: %d actors (overflow)\n\n", extreme_gpu_actors);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // CPU threads
    for (int i = 0; i < cpu_cores; i++) {
        workers[i].counters = extreme_counters_hybrid;
        workers[i].payloads = extreme_payloads;
        workers[i].start_actor = i * (extreme_cpu_actors / cpu_cores);
        workers[i].end_actor = (i == cpu_cores - 1) ? extreme_cpu_actors : (i + 1) * (extreme_cpu_actors / cpu_cores);
        workers[i].messages_per_actor = messages_per_actor;
        pthread_create(&threads[i], NULL, cpu_worker_thread, &workers[i]);
    }
    
    // GPU overflow
    clReleaseMemObject(d_counters);
    clReleaseMemObject(d_payloads);
    
    d_counters = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
                                sizeof(int) * extreme_gpu_actors, NULL, &err);
    d_payloads = clCreateBuffer(context, CL_MEM_READ_ONLY, 
                                sizeof(int) * extreme_gpu_actors * messages_per_actor, NULL, &err);
    
    free(gpu_counters_temp);
    gpu_counters_temp = (int*)calloc(extreme_gpu_actors, sizeof(int));
    
    clEnqueueWriteBuffer(queue, d_counters, CL_FALSE, 0, 
                        sizeof(int) * extreme_gpu_actors, gpu_counters_temp, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, d_payloads, CL_FALSE, 0, 
                        sizeof(int) * extreme_gpu_actors * messages_per_actor,
                        &extreme_payloads[extreme_cpu_actors * messages_per_actor], 0, NULL, NULL);
    
    offset = extreme_cpu_actors;
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_counters);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_payloads);
    clSetKernelArg(kernel, 2, sizeof(int), &messages_per_actor);
    clSetKernelArg(kernel, 3, sizeof(int), &offset);
    
    global_size = extreme_gpu_actors;
    clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
    
    for (int i = 0; i < cpu_cores; i++) {
        pthread_join(threads[i], NULL);
    }
    clFinish(queue);
    
    clEnqueueReadBuffer(queue, d_counters, CL_TRUE, 0, 
                       sizeof(int) * extreme_gpu_actors,
                       &extreme_counters_hybrid[extreme_cpu_actors], 0, NULL, NULL);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_extreme_hybrid = (end.tv_sec - start.tv_sec) + 
                                 (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_extreme_hybrid = extreme_messages / time_extreme_hybrid / 1e6;
    double extreme_speedup = time_extreme_cpu / time_extreme_hybrid;
    
    printf("    Time:       %.4f seconds\n", time_extreme_hybrid);
    printf("    Throughput: %.2f M msg/sec\n", throughput_extreme_hybrid);
    printf("    Speedup:    %.2fx vs CPU-only\n\n", extreme_speedup);
    
    // Summary
    printf("=== Summary ===\n");
    printf("Normal load (1M actors):\n");
    printf("  CPU-only:   %.2f M msg/sec\n", throughput_cpu_only);
    printf("  Hybrid:     %.2f M msg/sec (%.2fx)\n\n", throughput_hybrid, speedup);
    
    printf("Extreme load (2M actors):\n");
    printf("  CPU-only:   %.2f M msg/sec\n", throughput_extreme_cpu);
    printf("  Hybrid:     %.2f M msg/sec (%.2fx)\n\n", throughput_extreme_hybrid, extreme_speedup);
    
    if (extreme_speedup > 1.1) {
        printf("Result: GPU helps under EXTREME load (%.1fx speedup)\n", extreme_speedup);
        printf("Recommendation: Make GPU optional overflow handler\n");
    } else {
        printf("Result: CPU still faster even under extreme load\n");
        printf("Recommendation: Skip GPU, focus on CPU optimizations\n");
    }
    
    // Cleanup
    clReleaseMemObject(d_counters);
    clReleaseMemObject(d_payloads);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    
    free(extreme_counters_cpu);
    free(extreme_counters_hybrid);
    free(extreme_payloads);
    free(gpu_counters_temp);

cleanup:
    free(counters);
    free(payloads);
    free(cpu_only_counters);
    free(threads);
    free(workers);
    
    return 0;
}
