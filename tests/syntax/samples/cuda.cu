/* CUDA syntax sample: demonstrates all TS capture groups */
/* This file exercises every capture in cuda-highlights.scm */

// Line comment
// TODO: compute kernel

#include <cuda_runtime.h>
#include <stdio.h>
#include "mykernel.h"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

#ifdef USE_SHARED
#define SMEM_SIZE 1024
#endif

#if defined(__CUDA_ARCH__)
#pragma unroll
#endif

// CUDA qualifiers (@keyword.other -> white)
__global__ void vectorAdd(float* a, float* b, float* c, int n)
{
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < n)
    {
        c[idx] = a[idx] + b[idx];
    }
}

__device__ float deviceFunc(float x, float y)
{
    return x * y;
}

__host__ __device__ float hostDeviceFunc(float x)
{
    return x * x;
}

// CUDA built-in variables (@keyword.other -> white)
__global__ void kernel()
{
    int tx = threadIdx.x;
    int ty = threadIdx.y;
    int bx = blockIdx.x;
    int by = blockIdx.y;
    int bdx = blockDim.x;
    int bdy = blockDim.y;
    int gx = gridDim.x;
    int gy = gridDim.y;
    int ws = warpSize;
}

// CUDA memory qualifiers
__shared__ float sharedMem[256];
__constant__ float constData[64];
__managed__ int managedVar;
__restrict__ float* restrictPtr;

// CUDA synchronization (@keyword.other -> white)
__global__ void syncKernel()
{
    __shared__ float temp[256];
    temp[threadIdx.x] = 0.0f;
    __syncthreads();
    __threadfence();
}

// Keywords (@keyword -> yellow)
class CudaHelper {
public:
    explicit CudaHelper(int deviceId) {
        this->deviceId = deviceId;
    }
    virtual ~CudaHelper() = default;
protected:
    int deviceId;
private:
    bool initialized;
};

struct DeviceInfo {
    int major;
    int minor;
};

enum MemType { Host, Device, Managed };

namespace cuda_utils {
    using namespace std;
    template <typename T>
    T* allocate(int n) {
        T* ptr;
        return ptr;
    }
}

// true, false, nullptr, this (@keyword -> yellow)
bool flag = true;
bool off = false;
void* np = nullptr;

// Primitive types (@type -> yellow)
void func_void();
int func_int(int a);
float func_float(float f);
double func_double(double d);
unsigned int ui;
signed int si;
long l;

// Operators (@operator.word -> yellow)
int ops(int a, int b) {
    int r;
    r = a + b;
    r = a - b;
    r = a * b;
    r = a / b;
    r = a % b;
    r++;
    r--;
    r += a;
    r -= b;
    if (a == b) return 0;
    if (a != b) return 1;
    if (a < b) return -1;
    if (a > b) return 1;
    if (a <= b && a >= 0) return a;
    if (a || b) return a;
    if (!a) return b;
    r = a & b;
    r = a | b;
    r = a ^ b;
    r = ~a;
    r = a << 2;
    r = a >> 1;
    return r;
}

// Scope resolution
void scope_demo() {
    cuda_utils::allocate<float>(100);
}

// Strings (@string -> green)
const char* s1 = "Hello, CUDA!";
const char* s2 = "escape: \t\n\\\"";
const char* raw = R"(raw string)";

// Character literals (@string.special -> brightgreen)
char ch1 = 'A';
char ch2 = '\n';
char ch3 = '\\';

// null (@constant -> lightgray)
void* pnull = NULL;

// Labels (@label -> cyan)
void label_func() {
    goto done;
done:
    return;
}

// Delimiters (@delimiter -> brightcyan): . , :
// Semicolons (@delimiter.special -> brightmagenta): ;
// Arrow operator: ->

// Control flow
void control(int x) {
    switch (x) {
    case 0: break;
    default: break;
    }
    try { throw 1; } catch (int e) { }
    for (int i = 0; i < 10; i++) { continue; }
    while (x > 0) { x--; }
    do { x++; } while (x < 5);
}

// More keywords
typedef int Int32;
union DataUnion { int i; float f; };
volatile int vvar;
inline void ifunc() {}
static int svar;
extern int evar;
using IntVec = int;
struct Final final {};
void sized() { sizeof(int); }

// Host code launching kernel
int main() {
    float* d_a;
    float* d_b;
    float* d_c;
    int n = 1024;
    vectorAdd<<<GRID_SIZE(n), BLOCK_SIZE>>>(d_a, d_b, d_c, n);
    return 0;
}

// Comments (@comment -> brown)
/* Block comment */
// Line comment
