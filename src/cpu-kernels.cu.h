// This contains the sequential C versions to validate
#ifndef _CPU_KERNELS
#define _CPU_KERNELS
#include <math.h>

// HELPERS
void seq_transpose(float* X_in, float* X_out, int rows, int cols){
    for (int r = 0; r < rows; r++){
        for (int c = 0; c < cols; c++){
            // X_out[c][r] = X_in[r][c]
            X_out[c * rows + r] = X_in[r*cols + c];
        }
    }
}
// This is hardcoded to use the predicate "isnan", because we don't really need it for
// anything else
// Returns number of valid values
int seq_filterPadWithKeys(float* arr, float* Rs, int* Ks, int n){
    int c = 0;
    for(int i = 0; i < n; i++){
        if (!isnan(arr[i])){
            Rs[c] = arr[i];
            Ks[c] = i;
            c++;
        }
    }
    for(int i = c; i < n; i++){
        Rs[i] = NAN;
        Ks[i] = 0;
    }
    return c;
}
//--- Creates the interpolation matrix ---
// K is the number of rows
// N is the number of columns 
// f is the frequency of the observations
// X_out is the resulting interpolation matrix, which has size KxN
//  ecach column, x_t, will correspond to the data generated for a spefic date t
void seq_mkX(int K, int N, float f, int* mappingIndices, float* X_out){
    //Loops through the rows
    for (int i = 0; i < K; i++)
    {         
        //Loops through the indices in each row and creates the elements
        for(int t = 0; t < N; t++){
            //Calculates the current index for X_out
            int cur_ind = N*i + t;
            if(i == 0){
                //This correspond to the first index of each x_t, which should always be 1
                X_out[cur_ind] = 1;
            } else if (i == 1) {
                //This correspond the second index of each x_t
                X_out[cur_ind] = (float)mappingIndices[t]; 
            } else {
                //Calculates Ft(j)
                float i_ = (float)(i / 2);
                float j_ = ((float)mappingIndices[t]);
                float F = (2.0f*M_PI*i_*j_)/f;  
                if(i% 2 == 0){
                    X_out[cur_ind] = sinf(F);
                }else{
                    X_out[cur_ind] = cosf(F);
                }
            }
        }
    }
}


float seq_dotprodFilt(float* x, float* y, float* vec, int n){
    float sum = 0.0f;
    for(int i = 0; i < n; i++){
        if (!isnan(vec[i]))
            sum += x[i] * y[i];
    }
    return 0.0f;
}
float seq_dotprod(float* x, float* y, int n){
    float sum = 0.0f;
    for(int i = 0; i < n; i++){
        sum += x[i] * y[i];
    }
    return sum;
}

// --- Filtered Matrix - Matrix Multiplication ---
// Should multiply X*X^t and filter out the rows, where y is NAN
// X is a KxN matrix
// X_t is a NxK matrix
// y is a matrix of size m*N
// Output is mxKxK matrix
void seq_mmMulFilt(float* X, float* X_t, float* y,
        float* M, int pixels, int n, int p, int m){
    for(int pix = 0; pix < pixels; pix++){
        for(int i = 0; i < n; i++){
            for (int j = 0; j < m; j++){
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
    }
}
// --- Matrix - Matrix Multiplication ---
// Should multiply X*X^t and filter out the rows, where y is NAN
// X is a KxN matrix
// X_t is a NxK matrix
// y is a matrix of size m*N
// Output is mxKxK matrix
void seq_mmMul(float* X, float* X_t, float* y,
        float* M, int pixels, int n, int p, int m){
    for(int pix = 0; pix < pixels; pix++){
        for(int i = 0; i < n; i++){
            for (int j = 0; j < m; j++){
                float accum = 0.0f;
                for (int k = 0; k < p; k++){
                    float a = X[I2(i,k,p)];
                    float b = X_t[I2(k,j,m)];
                    accum += a*b;
                }
                M[I3(pix, i, j, n, m)] = accum;
            }
        }
    }
}


// --- Invert a matrix --- 
// Allocate the [K][2K] array in here
// X_sqr is a mxKxK matrix 
// A is the output and also a mxKxK matrix. 768b, 192 floats
void seq_matInv (float* X_sqr, float* A, int matrices, int height){
    int width = 2*height;
    // And will be shared in the grid blocks
    // 384 float
    float* Ash = (float*)malloc(height*width*sizeof(float));// This will be array we gauss-jordan
    for(int i = 0; i < matrices; i++){
        for(int k1 = 0; k1 < height; k1++){
            for(int k2 = 0; k2 < width; k2++){
                // Fill up the tmp array
                //Ash[I2(k1, k2, width)] = 0.0f;
                int ind = I2(k1, k2, width);
                int ind3= I3(i, k1, k2, height, height);
                Ash[ind] = (k2 < height) ?
                    X_sqr[ind3] : 
                    (k2 == height + k1);
            }
        }
        //printf("Filled Ash %d times, which is less than %d\n",i, matrices);
        for(int i_=0;i_<height;i_++)
        {
            float curEl = Ash[I2(i_, i_, width)];
            if(Ash[I2(i_,i_,width)] == 0.0)
            {
                continue;
            }
            for(int j=0;j<height;j++)
            {
                if(i_!=j)
                {
                     float ratio = Ash[I2(j,i_,width)]/Ash[I2(i_,i_,width)];
                     for(int k=0;k<width;k++)
                     {
                         Ash[I2(j,k,width)] = Ash[I2(j,k,width)] - ratio*Ash[I2(i_,k,width)];
                     }
                }
            }
            for(int c = 0; c < width; c++){
                Ash[I2(i_, c, width)] /= curEl;
            }
        }
        for(int k1 = 0; k1 < height; k1++){
            for(int k2 = 0; k2 < height; k2++){
                A[I3(i, k1, k2, height, height)] = Ash[I2(k1, height+k2, width)];
            }
        }
    }
}

// --- Filtered Matrix * Vector multiplication ---
// Calculate X*y
// X is assumed to a PxKxN matrix
// Y is al
// Output is y_out and will be vector of size MxK
void seq_mvMulFilt(float* X, float* y, float* y_out, int pixels, int height, int width){ 
    for(int pix = 0; pix < pixels; pix++){
        for(int i = 0; i < height; i++){
            float accum = 0.0f;
            for (int k = 0; k < width; k++){
                float a = X[I2(i,k,width)];
                float b = y[I2(pix,k,width)];
                if (!isnan(b))
                    accum += a*b;
            }
            y_out[I2(pix, i, height)] = accum;
        }
    }
}


// --- UnFiltered matrix * vector multiplication - calculates the prediction --- 
// Used for calculating Beta, and y_preds
// X is an mxKxK matrix
// y is an vector of size mxK
// Ouput will be y_out, which an mxK vector
void seq_mvMul(float* X, float* y, float* y_out, int pixels, int height, int width){ 
    for(int pix = 0; pix < pixels; pix++){
        for(int i = 0; i < height; i++){
            float accum = 0.0f;
            for (int k = 0; k < width; k++){
                float a = X[I3(pix,i,k,width, width)];
                float b = y[I2(pix,k,width)];
                accum += a*b;
            }
            y_out[I2(pix, i, height)] = accum;
        }
    }
}
// Another mvMul
void seq_mvMul2(float* X, float* y, float* y_out, int pixels, int height, int width){ 
    for(int pix = 0; pix < pixels; pix++){
        for(int t = 0; t < height; t++){
            float accum = 0.0f;
            for(int i = 0; i < height; i++){
                float a = X[I2(pix, i, height)];
                float b = y[I2(pix, i, height)];
                accum += a*b;
            }

            y_out[I2(pix, t, width )] = accum;
        }
    }
}

// --- K5 Calculates Y - Y_pred --- 
// Y is a mxN
// y_preds is mxN
// R is mxN
// K is mxN
// Nss is m vector
void seq_YErrorCalculation(float* Y, float* Ypred, float* R, int* K, int* Ns, int m, int N){
    float y_err_tmp[N];
    // For all pixels
    for(int pix = 0; pix < m; pix++){
        // Calculate y - ypred 
        for(int i = 0; i < N; i++){
            float ye = Y[I2(pix, i, N)];
            float yep = Ypred[I2(pix, i, N)];
            y_err_tmp[i] = (isnan(ye)) ? ye : (ye - yep);
        }
        // and put in a padded array
        int n = seq_filterPadWithKeys(y_err_tmp, &R[I2(pix, 0, N)], &K[I2(pix, 0, N)], N);
        Ns[pix] = n;
    }
}




// --- Kernel 6 ---
// Creates the lists hs, nss and sigmas, whih will be used in later calculatations
// yh       is mxn
// y_errors is mxN
// out:
// sigmas: 1xM
// hs    : 1xM
// ns    : 1xM
void seq_NSSigma(float* Y_errors, float* Y_historic, 
         float* sigmas, int* hs, int* nss, int N, int n, int m, int k, float hfrac){
    for(int pix = 0; pix < m; pix++){
        // yh   = Yh[pix*n]
        // Yerr = Yerror[pix*N]
        int ns = 0;
        for(int i = 0; i < n; i++){
            if (!isnan(Y_historic[I2(pix, i, n)])) ns++;
        }
        float sigma = 0.0f;
        for(int i = 0; i < ns; i++){
            float tmp = Y_errors[I2(pix, i, N)];
            sigma += tmp*tmp;
        }
        sigma = sqrtf(sigma / ((float)(ns - k)));
        float h = (int) (((float)ns) * hfrac);
        sigmas[pix] = sigma;
        nss[pix]    = ns;
        hs[pix]     = h;
        
    }
}

// --- Kernel 7 ---
// 
// Map
// y_errs   = mxN
// nss      = 1xm
// hs       = 1xm
void seq_msFst(float* y_error, int* ns, int* hs, float* MO_fsts, int m, int N, int hMax){
    for(int pix = 0; pix < m; pix++){
        float sum = 0;
        for(int i = 0; i < hMax; i++){
            if (i < hs[pix])
                sum += y_error[I2(pix, i+ns[pix]-hs[pix]+1, N)];
        }
        MO_fsts[pix] = sum;
    }
}

// Ns          = mx1
// nss         = mx1
// sigmas      = mx1
// hs          = mx1
// MO_fsts     = mx1
// y_errors    = mxN
// val_inds(k) = mxN
// BOUND       = 1x(N-n)
void seq_mosum(int* Nss, int* nss, float* sigmas, int* hs, float* MO_fsts, float* y_errors,
        int* val_inds, float* BOUND, int* breaks, int N, int n, int m){
    for(int pix = 0; pix < m; pix++){
        // Calculates Moving sums
        float mo[N-n];
        for(int j = 0; j < N-n; j++){
            if (j >= Nss[pix]-nss[pix]){
                mo[j] = mo[j-1];
            }
            else if(j == 0){
                mo[j] = MO_fsts[pix];
            } else{
                // This eliminates the scan. We can't do this in parallel though
                mo[j] = mo[j-1] + (-y_errors[I2(pix, nss[pix]-hs[pix]+j, N)] + 
                        y_errors[I2(pix, nss[pix]+j, N)]);
            }
        }
        // Get'in siggy with it
        for(int j = 0; j < N - n; j++){
            mo[j] = mo[j] / (sigmas[pix] * (sqrtf((float)nss[pix])));
        }
        // Find the first break
        int fbreak = -1;
        breaks[pix] = -1;
        for(int j = 0; j < N - n; j++){
            if (j < (Nss[pix] - nss[pix]) && !(isnan(mo[j]))){
                if (fabs(mo[j]) > BOUND[j]){
                    fbreak = val_inds[j];
                    breaks[pix] = val_inds[j];
                    break;
                }
                    
            }
        }
    }
}


#endif



