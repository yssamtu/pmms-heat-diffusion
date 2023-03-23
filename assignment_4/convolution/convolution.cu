#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cuda_runtime.h>
// #include <cstring>
#include "timer.h"

enum {
    image_height = 1024,
    image_width = 1024,
    filter_height = 5,
    filter_width = 5,
    border_height =  filter_height & -2,
    border_width = filter_width & -2,
    input_height = image_height + border_height,
    input_width = image_width + border_width,
    block_size_x = 32,
    block_size_y = 16,
    SEED = 1234
};

using std::isnan;
using std::fprintf;
using std::printf;
using std::puts;
using std::calloc;
using std::free;
using std::malloc;
using std::cout;
using std::endl;

static void convolutionSeq(float *output, float *input, float *filter);
__global__ void convolution_kernel_naive(float *output, float *input, float *filter);
static void convolutionCUDA(float *output, float *input, float *filter);
static int compare_arrays(float *a1, float *a2, int n);

int main()
{
    // Allocate arrays and fill them
    float *input = static_cast<float *>(malloc(input_height * input_width * sizeof(float)));
    float *output1 = static_cast<float *>(calloc(image_height * image_width, sizeof(float)));
    float *output2 = static_cast<float *>(calloc(image_height * image_width, sizeof(float)));
    float *filter = static_cast<float *>(malloc(filter_height * filter_width * sizeof(float)));

    for (int i = 0; i < input_height * input_width; ++i) {
        input[i] = static_cast<float>(i % SEED);
    }

// This is specific for a W == H smoothening filter i, where W and H are odd.
    for (int i = 0; i < filter_height * filter_width; ++i) {
        filter[i] = 1.0f;
    }

    for (int i = filter_width + 1; i < (filter_height - 1) * filter_width; ++i) {
	    if (i % filter_width > 0 && i % filter_width < filter_width - 1)
            filter[i] += 1.0f;
    }

    filter[filter_width * filter_height >> 1] = 3.0f;

// End initialisation

    // Measure the CPU function
    convolutionSeq(output1, input, filter);
    // Measure the GPU function
    // convolutionCUDA(output2, input, filter);


    // // Check the result
    // int errors += compare_arrays(output1, output2, image_height * image_width);
    // if (errors > 0)
    //     printf("TEST FAILED! %d errors!\n", errors);
    // else
    //     puts("TEST PASSED!");


    free(input);
    free(output1);
    free(output2);
    free(filter);

    return 0;
}

static void convolutionSeq(float *output, float *input, float *filter)
{
    // For each pixel in the output image

  timer sequentialTime = timer("Sequential");

  sequentialTime.start();

    for (int y = 0; y < image_height; ++y) {
        for (int x = 0; x < image_width; ++x) {
	        output[y * image_width + x] = 0.0f;
            // For each filter weight
            for (int i = 0; i < filter_height; ++i) {
                for (int j = 0; j < filter_width; ++j) {
                    output[y * image_width + x] += input[(y + i) * input_width + x + j] * filter[i * filter_width + j];
                }
            }
	        output[y * image_width + x] /= 35.0f;
        }
    }

  sequentialTime.stop();
  cout << "convolution (sequential): \t\t" << sequentialTime << endl;

}

__global__ void convolution_kernel_naive(float *output, float *input, float *filter)
{
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
}

static void convolutionCUDA(float *output, float *input, float *filter)
{
    // Memory allocation
    float *d_input = nullptr;
    cudaError_t err = cudaMalloc(&d_input, input_height * input_width * sizeof(float));
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMalloc d_input: %s\n", cudaGetErrorString(err));
    float *d_output = nullptr;
    err = cudaMalloc(&d_output, image_height * image_width * sizeof(float));
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMalloc d_output: %s\n", cudaGetErrorString(err));
    float *d_filter = nullptr;
    err = cudaMalloc(&d_filter, filter_height * filter_width * sizeof(float));
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMalloc d_filter: %s\n", cudaGetErrorString(err));

    timer memoryTime = timer("memoryTime");
    memoryTime.start();
    // Host to device
    err = cudaMemcpy(d_input, input, input_height * input_width * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMemcpy host to device input: %s\n", cudaGetErrorString(err));
    err = cudaMemcpy(d_filter, filter, filter_height * filter_width * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMemcpy host to device filter: %s\n", cudaGetErrorString(err));

    // Zero the result array
    err = cudaMemset(d_output, 0, image_height * image_width * sizeof(float));
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMemset output: %s\n", cudaGetErrorString(err));
    memoryTime.stop();
    // Set up the grid and thread blocks
    // Thread block size
    dim3 threads(block_size_x, block_size_y);
    // Problem size divided by thread block size rounded up
    dim3 grid(static_cast<unsigned>(ceilf(image_width / static_cast<float>(threads.x))), static_cast<unsigned>(ceilf(image_height / static_cast<float>(threads.y))));

    // Measure the GPU function
    timer kernelTime = timer("kernelTime");
    kernelTime.start();
    convolution_kernel_naive<<<grid, threads>>>(d_output, d_input, d_filter);
    cudaDeviceSynchronize();
    kernelTime.stop();

    // Check to see if all went well
    err = cudaGetLastError();
    if (err != cudaSuccess)
        fprintf(stderr, "Error during kernel launch convolution_kernel: %s\n", cudaGetErrorString(err));

    // Copy the result back to host memory
    memoryTime.start();
    err = cudaMemcpy(output, d_output, image_height * image_width * sizeof(float), cudaMemcpyDeviceToHost);
    memoryTime.stop();
    if (err != cudaSuccess)
        fprintf(stderr, "Error in cudaMemcpy device to host output: %s\n", cudaGetErrorString(err));

    err = cudaFree(d_input);
    if (err != cudaSuccess)
        fprintf(stderr, "Error in freeing d_input: %s\n", cudaGetErrorString(err));
    err = cudaFree(d_output);
    if (err != cudaSuccess)
        fprintf(stderr, "Error in freeing d_output: %s\n", cudaGetErrorString(err));
    err = cudaFree(d_filter);
    if (err != cudaSuccess)
        fprintf(stderr, "Error in freeing d_filter: %s\n", cudaGetErrorString(err));

    cout << "convolution (kernel): \t\t" << kernelTime << endl;
    cout << "convolution (memory): \t\t" << memoryTime << endl;

}

static int compare_arrays(float *a1, float *a2, int n)
{
    int errors = 0;
    int print = 0;

    for (int i = 0; i < n; ++i) {

        if (isnan(a1[i]) || isnan(a2[i])) {
            ++errors;
            if (print < 10) {
                ++print;
                fprintf(stderr, "Error NaN detected at i=%d,\t a1= %10.7e \t a2= \t %10.7e\n", i, a1[i], a2[i]);
            }
        }

        float diff = (a1[i] - a2[i]) / a1[i];
        if (diff > 1e-6f) {
            ++errors;
            if (print < 10) {
                ++print;
                fprintf(stderr, "Error detected at i=%d, \t a1= \t %10.7e \t a2= \t %10.7e \t rel_error=\t %10.7e\n", i, a1[i], a2[i], diff);
            }
        }

    }

    return errors;
}
