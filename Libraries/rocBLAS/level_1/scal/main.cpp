// MIT License
//
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "cmdparser.hpp"
#include "example_utils.hpp"
#include "rocblas_utils.hpp"

#include <rocblas/rocblas.h>

#include <hip/hip_runtime.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

int main(const int argc, const char** argv)
{
    // Parse user inputs
    cli::Parser parser(argc, argv);
    parser.set_optional<float>("a", "alpha", 3.f, "Alpha scalar");
    parser.set_optional<int>("x", "incx", 1, "Increment for x vector");
    parser.set_optional<int>("n", "n", 5, "Size of vector");
    parser.run_and_exit_if_error();

    // Stride between consecutive values of input vector.
    const rocblas_int incx = parser.get<int>("x");
    if(incx <= 0)
    {
        std::cout << "Value of 'x' should be greater than 0" << std::endl;
        return 0;
    }

    // Number of elements in input vector.
    const rocblas_int n = parser.get<int>("n");
    if(n <= 0)
    {
        std::cout << "Value of 'n' should be greater than 0" << std::endl;
        return 0;
    }

    // Scalar value used for multiplication.
    const rocblas_float h_alpha = parser.get<float>("a");

    // Adjust the size of input vector for values of stride (incx) not equal to 1.
    const size_t size_x = n * incx;

    // Allocate memory for the host input vector.
    std::vector<float> h_x(size_x);

    // Initialize the values to the host vector to the increasing sequence 0, 1, 2, ...
    std::iota(h_x.begin(), h_x.end(), 0.f);

    std::cout << "Input Vector x: " << format_range(h_x.begin(), h_x.end()) << std::endl;

    // Initialize the values for vector h_x_gold, this vector will be used as a
    // Gold Standard to compare our results from rocBLAS SCAL function.
    std::vector<float> h_x_gold(h_x);

    // CPU function for SCAL.
    for(int i = 0; i < n; i++)
    {
        h_x_gold[i * incx] = h_alpha * h_x[i * incx];
    }

    // Use the rocBLAS API to create a handle.
    rocblas_handle handle;
    ROCBLAS_CHECK(rocblas_create_handle(&handle));

    // Allocate memory for the device vector.
    float* d_x{};
    HIP_CHECK(hipMalloc(&d_x, size_x * sizeof(float)));

    // Transfer data from host vectors to device vectors.
    HIP_CHECK(hipMemcpy(d_x, h_x.data(), sizeof(float) * size_x, hipMemcpyHostToDevice));

    // Enable passing the alpha parameter from a pointer to host memory.
    ROCBLAS_CHECK(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

    // SCAL calculation with single-precision on the device.
    ROCBLAS_CHECK(rocblas_sscal(handle, n, &h_alpha, d_x, incx));

    // Transfer the result from device vector to host vector,
    // which halts host execution until results are ready.
    HIP_CHECK(hipMemcpy(h_x.data(), d_x, sizeof(float) * size_x, hipMemcpyDeviceToHost));

    // Destroy the rocBLAS handle and release device memory.
    ROCBLAS_CHECK(rocblas_destroy_handle(handle));
    HIP_CHECK(hipFree(d_x));

    // Check the relative error between output generated by the rocBLAS API and the CPU.
    constexpr float eps    = 10.f * std::numeric_limits<float>::epsilon();
    unsigned int    errors = 0;
    for(size_t i = 0; i < size_x; i++)
    {
        errors += std::fabs(h_x[i] - h_x_gold[i]) > eps;
    }
    return report_validation_result(errors);
}
