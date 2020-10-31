// This contains the parallel kernels
#ifndef _CUDA_KERNELS
#define _CUDA_KERNELS

/// HELPERS

__global__ void sliceMatrix(float* X_in, float* X_out, int r_slice, int c_slice, int width){
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < r_slice && j < c_slice)
        X_out[I2(i, j, c_slice)] = X_in[I2(i, j, width)];
}
//
//
// Yanked from w3 t3
template <int T> 
__global__ void matTransposeTiledKer(float* A, float* B, int heightA, int widthA) {
    extern __shared__ char sh_mem1[];
    volatile float* tile = (volatile float*)sh_mem1;
    //__shared__ float tile[T][T+1];

    int x = blockIdx.x * T + threadIdx.x;
    int y = blockIdx.y * T + threadIdx.y;

    if( x < widthA && y < heightA )
        tile[threadIdx.y*(T+1) + threadIdx.x] = A[y*widthA + x];

    __syncthreads();

    x = blockIdx.y * T + threadIdx.x; 
    y = blockIdx.x * T + threadIdx.y;

    if( x < heightA && y < widthA )
        B[y*heightA + x] = tile[threadIdx.x*(T+1) + threadIdx.y];
}

// Kernels
// --- Kernel 1 ---
// Assumes 2d block dimensions
__global__ void gpu_mkX(int K, int N, float f, int* mappingIndices, float* X_out){
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    // Bounds check
    if (i < K && j < N){

        if (i == 0)
            X_out[I2(i,j,N)] = 1;
        else if (i == 1)
            X_out[I2(i,j,N)] = (float)mappingIndices[j];
        else{
            float i_ = (float)(i / 2);
            float j_ = ((float)mappingIndices[j]);
            float F = (2.0f*M_PI*i_*j_) / f;
            X_out[I2(i,j,N)] = (i % 2 == 0) ? sinf(F) : cosf(F);
        }
    }
}
// --- Kernel 2 ---
// Naive version
__global__ void gpu_mmMulFilt_naive(float* X, float* X_t, float* y,
        float* M, int pixels, int n, int p, int m){
    int pix = blockIdx.z * blockDim.z + threadIdx.z;
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    if ( pix < pixels && i < n && j < m){
        float accum = 0.0f;
        for (int k = 0; k < p; k++){
            float a = X[I2(i,k,p)];
            float b = X_t[I2(k,j,m)];
            if (!isnan(y[I2(pix, k, p)]))
                accum += a*b;
        }
        M[I3(pix, i, j, n, m)] = accum;
    }
}

// --- Kernel 3 ---
// Batched mat-inversion
// Requires a grid of ds->m blocks, with y = K, and x = 2K
template <int K> 
__global__ void gpu_batchMatInv(float* A, int m){
    int pix = blockIdx.x;
    // We only use threadIdx here since we get a block size of 16*8
    int k1 = threadIdx.y;
    int k2 = threadIdx.x;
    // Fill shared array
    __shared__ float Ash[K*2*K];
    Ash[I2(k1, k2, 2*K)] = (k2 < K) ? A[I3(pix, k1, k2, K, K)] : (k2 == K + k1);
    __syncthreads();
    __syncthreads();
    
}
//
int catchthis(int n){

    return n*n;
}
#endif
