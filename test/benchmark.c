#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32 // Windows-unique preprocessor directives for compatiblity
    #ifdef _WIN32_WINNT
        #undef _WIN32_WINNT
    #endif
    #define _WIN32_WINNT 0x0600 // Necessary for GetTickCount64
    #include <windows.h>
    // Timing defines
    #define TIMESTAMP_T long long
    #define GET_TIME(t_ptr) *(t_ptr) = GetTickCount64()
    #define DIFF_TIME_MS(t1_ptr, t0_ptr) *(t1_ptr)-*(t0_ptr)
#else
    // Socket defines
    #include <unistd.h>
    // Timing defines
    #define TIMESTAMP_T struct timespec
    #define GET_TIME(t_ptr) clock_gettime(CLOCK_MONOTONIC, t_ptr)
    #define DIFF_TIME_MS(t1_ptr, t0_ptr) \
        ((t1_ptr)->tv_sec-(t0_ptr)->tv_sec)*1000+((t1_ptr)->tv_nsec-(t0_ptr)->tv_nsec)/1000000
#endif

#ifndef NO_OPENCL
#include "../src/worker/worker.h"
#endif
#include "../src/mine_DUCO_S1.h"
#include "../src/mine_xxhash.h"

#define AVERAGE_COUNT 10

const unsigned char DUCO_S1_prefix[] = "c5a8787a9c6392560df7ed9c0f253f89092d8cd2";
const unsigned char DUCO_S1_target[] = "36c098fecc3e746247723bd74a3f0acf22b01985";
const int DUCO_S1_diff = 950000;
const int DUCO_S1_result = 23373972;

const unsigned char xxhash_prefix[] = "cd062b0305a3de29b1a8bc5fb928e48d849804c2";
const unsigned char xxhash_target[] = "9c31d2ef11adc708";
const int xxhash_diff = 750000;
const int xxhash_result = 21504575;

double average(double *arr, int arr_len){
    double sum = 0;
    for(int i = 0; i < arr_len; i++) sum += arr[i];
    return sum/arr_len;
}

double std_dev(double *arr, int arr_len){
    double arr_avg = average(arr, arr_len);
    double sum = 0;
    for(int i = 0; i < arr_len; i++){
        double diff = arr[i]-arr_avg;
        sum += diff*diff;
    }
    return sqrt(sum/(arr_len-1));
}

int main(){
    long nonce;
    TIMESTAMP_T t0, t1;
    int diff_ms_arr[AVERAGE_COUNT];
    double megahash_arr[AVERAGE_COUNT], megahash, megahash_error;

    
    #ifndef NO_OPENCL
    printf("Benchmarking mine_DUCO_S1_OpenCL... \n");

    char *filenames[4] = {
        "OpenCL/buffer_structs_template.cl",
        "OpenCL/lookup_tables.cl",
        "OpenCL/sha1.cl",
        "OpenCL/duco_s1.cl"
    };
    char gpu_name_buffer[128];
    int n_gpus = count_OpenCL_devices(CL_DEVICE_TYPE_GPU);
    cl_device_id *gpu_ids = (cl_device_id*)malloc(n_gpus*sizeof(cl_device_id));
    worker_ctx *gpu_ctxs = (worker_ctx*)malloc(n_gpus*sizeof(worker_ctx));
    get_OpenCL_devices(gpu_ids, n_gpus, CL_DEVICE_TYPE_GPU);
    init_OpenCL_workers(gpu_ctxs, gpu_ids, n_gpus);

    for(int i = 0; i < n_gpus; i++){
        clGetDeviceInfo(gpu_ids[i], CL_DEVICE_NAME, 128, gpu_name_buffer, NULL);
        printf("Benchmarking GPU %d (%s)... ", i, gpu_name_buffer);

        build_OpenCL_worker_source(&gpu_ctxs[i], gpu_ids[i], filenames, 4);
        build_OpenCL_worker_kernel(&gpu_ctxs[i], 524288);

        for(int j = 0; j < AVERAGE_COUNT; j++){
            GET_TIME(&t0);
            nonce = mine_DUCO_S1_OpenCL(DUCO_S1_prefix, 40, DUCO_S1_target, DUCO_S1_diff, &gpu_ctxs[i]);
            GET_TIME(&t1);
            diff_ms_arr[j] = DIFF_TIME_MS(&t1, &t0);
        }

        for(int i = 0; i < AVERAGE_COUNT; i++)
            megahash_arr[i] = (double)nonce*1000/diff_ms_arr[i]/1000000;
        megahash = average(megahash_arr, AVERAGE_COUNT);
        megahash_error = std_dev(megahash_arr, AVERAGE_COUNT);

        if(nonce == DUCO_S1_result) printf("Passed, ");
        else printf("Failed, ");
        printf("with speed %.2f +/- %.4f MH/s\n", megahash, megahash_error);
        
        deconstruct_OpenCL_worker_kernel(&gpu_ctxs[i]);
    }

    free(gpu_ids);
    free(gpu_ctxs);
    #endif


    printf("Benchmarking mine_DUCO_S1... ");

    for(int i = 0; i < AVERAGE_COUNT; i++){
        GET_TIME(&t0);
        nonce = mine_DUCO_S1(DUCO_S1_prefix, 40, DUCO_S1_target, DUCO_S1_diff);
        GET_TIME(&t1);
        diff_ms_arr[i] = DIFF_TIME_MS(&t1, &t0);
    }
    for(int i = 0; i < AVERAGE_COUNT; i++)
        megahash_arr[i] = (double)nonce*1000/diff_ms_arr[i]/1000000;
    megahash = average(megahash_arr, AVERAGE_COUNT);
    megahash_error = std_dev(megahash_arr, AVERAGE_COUNT);

    if(nonce == DUCO_S1_result) printf("Passed, ");
    else printf("Failed, ");
    printf("with speed %.2f +/- %.4f MH/s\n", megahash, megahash_error);
    printf("Approximate four-core performance: %.2f +/- %.4f MH/s\n", megahash*4, megahash_error*4);


    printf("Benchmarking xxhash... ");

    for(int i = 0; i < AVERAGE_COUNT; i++){
        GET_TIME(&t0);
        nonce = mine_xxhash(xxhash_prefix, 40, xxhash_target, xxhash_diff);
        GET_TIME(&t1);
        diff_ms_arr[i] = DIFF_TIME_MS(&t1, &t0);
    }
    for(int i = 0; i < AVERAGE_COUNT; i++)
        megahash_arr[i] = (double)nonce*1000/diff_ms_arr[i]/1000000;
    megahash = average(megahash_arr, AVERAGE_COUNT);
    megahash_error = std_dev(megahash_arr, AVERAGE_COUNT);

    if(nonce == xxhash_result) printf("Passed, ");
    else printf("Failed, ");
    printf("with speed %.2f +/- %.4f MH/s\n", megahash, megahash_error);
    printf("Approximate four-core performance: %.2f +/- %.4f MH/s\n", megahash*4, megahash_error*4);

    return 0;
}