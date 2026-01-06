// OpenCL GPU Benchmark - Works with GCC, no Visual Studio needed
// Supports: NVIDIA, AMD, Intel GPUs

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

// OpenCL kernel source (runs on GPU)
const char* kernel_source = 
"__kernel void actor_step_kernel(\n"
"    __global int* counters,\n"
"    __global int* payloads,\n"
"    int messages_per_actor\n"
") {\n"
"    int actor_id = get_global_id(0);\n"
"    \n"
"    for (int m = 0; m < messages_per_actor; m++) {\n"
"        counters[actor_id] += payloads[actor_id * messages_per_actor + m];\n"
"    }\n"
"}\n";

// Check OpenCL errors
void check_cl_error(cl_int err, const char* msg) {
    if (err != CL_SUCCESS) {
        fprintf(stderr, "OpenCL Error %d: %s\n", err, msg);
        exit(1);
    }
}

// CPU baseline
void process_cpu(int* counters, int* payloads, int actor_count, int messages_per_actor) {
    for (int i = 0; i < actor_count; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            counters[i] += payloads[i * messages_per_actor + m];
        }
    }
}

int main(int argc, char* argv[]) {
    int num_actors = 100000;
    int messages_per_actor = 10;
    
    if (argc >= 2) num_actors = atoi(argv[1]);
    if (argc >= 3) messages_per_actor = atoi(argv[2]);
    
    printf("=== GPU Acceleration Benchmark (OpenCL) ===\n");
    printf("Actors: %d\n", num_actors);
    printf("Messages per actor: %d\n", messages_per_actor);
    
    long long total_messages = (long long)num_actors * messages_per_actor;
    printf("Total messages: %lld\n\n", total_messages);
    
    // Initialize data
    int* counters = (int*)calloc(num_actors, sizeof(int));
    int* payloads = (int*)malloc(sizeof(int) * total_messages);
    
    for (long long i = 0; i < total_messages; i++) {
        payloads[i] = 1;
    }
    
    // Test 1: CPU Baseline
    printf("Test 1: CPU Baseline\n");
    
    int* cpu_counters = (int*)calloc(num_actors, sizeof(int));
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    process_cpu(cpu_counters, payloads, num_actors, messages_per_actor);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_cpu = (end.tv_sec - start.tv_sec) + 
                      (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_cpu = total_messages / time_cpu / 1e6;
    
    printf("  Time:       %.4f seconds\n", time_cpu);
    printf("  Throughput: %.2f M msg/sec\n\n", throughput_cpu);
    
    long long cpu_sum = 0;
    for (int i = 0; i < num_actors; i++) {
        cpu_sum += cpu_counters[i];
    }
    free(cpu_counters);
    
    // Test 2: GPU with OpenCL
    printf("Test 2: GPU (OpenCL)\n");
    
    // Get platform
    cl_platform_id platform;
    cl_uint num_platforms;
    cl_int err = clGetPlatformIDs(1, &platform, &num_platforms);
    
    if (err != CL_SUCCESS || num_platforms == 0) {
        printf("  ERROR: No OpenCL platforms found!\n");
        printf("  This means:\n");
        printf("    - No GPU drivers installed, OR\n");
        printf("    - OpenCL runtime not installed\n\n");
        printf("  Install:\n");
        printf("    NVIDIA: GeForce/CUDA drivers include OpenCL\n");
        printf("    AMD: Radeon drivers include OpenCL\n");
        printf("    Intel: Intel GPU drivers include OpenCL\n\n");
        printf("  Skipping GPU test...\n");
        goto cleanup;
    }
    
    // Get GPU device
    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    
    if (err != CL_SUCCESS) {
        printf("  WARNING: No GPU found, trying CPU device...\n");
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
        check_cl_error(err, "Failed to get OpenCL device");
    }
    
    // Print device info
    char device_name[128];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    printf("  Device: %s\n", device_name);
    
    cl_ulong mem_size;
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
    printf("  Memory: %.2f GB\n", mem_size / 1e9);
    
    cl_uint compute_units;
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, NULL);
    printf("  Compute Units: %u\n\n", compute_units);
    
    // Create context and command queue
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_cl_error(err, "Failed to create context");
    
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
    check_cl_error(err, "Failed to create command queue");
    
    // Create and build program
    cl_program program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    check_cl_error(err, "Failed to create program");
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        fprintf(stderr, "Build error:\n%s\n", log);
        free(log);
        exit(1);
    }
    
    // Create kernel
    cl_kernel kernel = clCreateKernel(program, "actor_step_kernel", &err);
    check_cl_error(err, "Failed to create kernel");
    
    // Create buffers
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    cl_mem d_counters = clCreateBuffer(context, CL_MEM_READ_WRITE, 
                                       sizeof(int) * num_actors, NULL, &err);
    check_cl_error(err, "Failed to create counter buffer");
    
    cl_mem d_payloads = clCreateBuffer(context, CL_MEM_READ_ONLY, 
                                       sizeof(int) * total_messages, NULL, &err);
    check_cl_error(err, "Failed to create payload buffer");
    
    // Transfer to GPU
    err = clEnqueueWriteBuffer(queue, d_counters, CL_TRUE, 0, 
                               sizeof(int) * num_actors, counters, 0, NULL, NULL);
    check_cl_error(err, "Failed to write counters");
    
    err = clEnqueueWriteBuffer(queue, d_payloads, CL_TRUE, 0, 
                               sizeof(int) * total_messages, payloads, 0, NULL, NULL);
    check_cl_error(err, "Failed to write payloads");
    
    // Set kernel arguments
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_counters);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_payloads);
    err |= clSetKernelArg(kernel, 2, sizeof(int), &messages_per_actor);
    check_cl_error(err, "Failed to set kernel args");
    
    // Execute kernel
    size_t global_size = num_actors;
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
    check_cl_error(err, "Failed to execute kernel");
    
    // Wait for completion
    clFinish(queue);
    
    // Transfer back
    err = clEnqueueReadBuffer(queue, d_counters, CL_TRUE, 0, 
                              sizeof(int) * num_actors, counters, 0, NULL, NULL);
    check_cl_error(err, "Failed to read results");
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_gpu = (end.tv_sec - start.tv_sec) + 
                      (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_gpu = total_messages / time_gpu / 1e6;
    double speedup = time_cpu / time_gpu;
    
    printf("  Time:       %.4f seconds\n", time_gpu);
    printf("  Throughput: %.2f M msg/sec\n", throughput_gpu);
    printf("  Speedup:    %.2fx vs CPU\n\n", speedup);
    
    // Verify
    long long gpu_sum = 0;
    for (int i = 0; i < num_actors; i++) {
        gpu_sum += counters[i];
    }
    
    if (gpu_sum != cpu_sum) {
        printf("  ERROR: GPU result (%lld) != CPU result (%lld)\n", gpu_sum, cpu_sum);
    } else {
        printf("  Status:     PASS (verified)\n\n");
    }
    
    // Cleanup
    clReleaseMemObject(d_counters);
    clReleaseMemObject(d_payloads);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    
    // Summary
    printf("=== Summary ===\n");
    if (speedup > 1.5) {
        printf("Result: GPU is %.2fx faster - BENEFICIAL!\n", speedup);
    } else if (speedup < 0.8) {
        printf("Result: CPU is faster due to transfer overhead\n");
    } else {
        printf("Result: CPU and GPU roughly equivalent\n");
    }
    
cleanup:
    free(counters);
    free(payloads);
    
    return 0;
}
