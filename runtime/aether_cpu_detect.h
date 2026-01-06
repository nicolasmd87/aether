#ifndef AETHER_CPU_DETECT_H
#define AETHER_CPU_DETECT_H

typedef struct {
    int avx_supported;
    int avx2_supported;
    int avx512f_supported;
    int fma_supported;
    char cpu_brand[49];
    int num_cores;
} CPUInfo;

// Get CPU information (cached after first call)
const CPUInfo* cpu_get_info(void);

// Quick feature checks
int cpu_has_avx2(void);
int cpu_has_avx512(void);

// Display CPU info
void cpu_print_info(void);

// Get recommended core count for runtime
int cpu_recommend_cores(void);

#endif // AETHER_CPU_DETECT_H
