/* LU Decomposition:
 * ====================
 * - general description/optimization
 * - some speedup measurements
 * - hostname used to obtain these results hostname --fqdn
 *
 * Authors:
 * ====================
 * team07:
 * Gerd Zellweger (zgerd@student.ethz.ch)
 * Boris Bluntschli (borisb@student.ethz.ch)
 *
 
decompose_matrix 
================
size=1024
------------------------------
1 thread:  1.45s                [1.45, 1.49, 1.42]
2 threads: 1.01s (143% speedup) [0.92, 1.12, 1.00]
4 threads: 1.30s (111% speedup) [1.23, 1.38, 1.31] 

 
check_matrix (blocked implementation)
============
------------------------------
size=1024
------------------------------
1 thread:  3.23s [3.76, 3.92, 3.73]
2 threads: 2.02s [2.00, 2.33, 1.94] (159% speedup)
4 threads: 1.94s (166% speedup)

------------------------------
size=512
------------------------------
1 thread:  3.23s [3.76, 3.92, 3.73]
2 threads: 2.02s [2.00, 2.33, 1.94] (159% speedup)
4 threads: 1.94s (166% speedup)



 */

// <config>

#define NUM_THREADS 2

// </config>
 
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define index(i, j) ((i)*size+(j))

/** 
 * Decomposes a matrix in-situ into its L and U components
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 */
void decompose_matrix(double* matrix, int size) {
    omp_set_num_threads(NUM_THREADS);

	int k, i;
    
	for(k=0; k < size; k++) {

		int j;
		double divisor = matrix[index(k, k)];

		// (k, k+1) to (k, size-1)
		for(j=k+1; j < size; j++) {
			matrix[index(k, j)] = matrix[index(k, j)] / divisor;
		}

        // (k+1, k+1) to (size-1, size-1)
		#pragma omp parallel for private(i, j)
		for(i=k+1; i<size; i++) {
			int j;
            double d = matrix[index(i, k)];
			for(j=k+1; j<size; j++) {
				matrix[index(i, j)] = matrix[index(i, j)] - d * matrix[index(k, j)];
                
                
			}
		}

	}

}

/**
 * Checks if (l*u)_ij == matrix_ij
 * @param lu     pointer to the lu matrix
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 * @param i      row to check
 * @param j      column to check
 * @return       true if l*u=matrix, false otherwise
 */
inline int check_matrix_entry(double *lu, double *matrix, int size, int i, int j) {
	// we take i-th row and multiply by j-th column
	// Aij = l[i:] dot u[:j]

	double result = 0.0;

	/* As we're dealing with two diagonal matrices, we can ignore
	entries x_kl for which k > min(i, j) or l > min(i, j) */
	int min = i < j ? i : j;

    int k_max = min == j ? j - 1 : min;
    
	int k;
	for(k = 0; k <= k_max; ++k) {
		result += lu[index(i, k)]*lu[index(k, j)];
	}

	// Uii is always == 1 and not explicitly stored
	if(j <= min) {
		result += lu[index(i, j)];
	}

	double diff = result - matrix[index(i, j)];
    
	return abs(diff) == 0.0;
}

/** 
 * Checks if the computation of l*u is the same as matrix
 * @param lu     pointer to the lu matrix
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 * @return       true if l*u=matrix, false otherwise
 */
int check_matrix(double *lu, double *matrix, int size) {
    omp_set_num_threads(NUM_THREADS);

    int result = 1;
	
    {
        int i, j, r;
        #pragma omp parallel for schedule(static, 1) private(i, j, r)
        for(i = 0; i < size; ++i) {
            r = 1;
            for(j = 0; j < size; ++j) {
                r &= check_matrix_entry(lu, matrix, size, i, j);
            }
            if(!r) {
                result = 0;
            }
        }
    }
	return result;
}

/** 
 * Checks if the computation of l*u is the same as matrix
 * @param lu     pointer to the lu matrix
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 * @return       true if l*u=matrix, false otherwise
 */
int check_matrix_blocked(double *lu, double *matrix, int size) {
    omp_set_num_threads(NUM_THREADS);
    
    // As the only operation on result is setting it to 0, we don't need to synchronize access to it
    int result = 1;
    
    // We split the matrix into blocks of size `block_size` in the hope of getting better caching behavior
    int block_size = 32;
    int num_blocks = (size + block_size - 1) / block_size;
    
    int block;    
    #pragma omp parallel for private(block) schedule(static, 1)
    for(block = 0; block < num_blocks*num_blocks; ++block)
    {
        // Calculate the x and y coordinates of the block
        int xblock = block % num_blocks;
        int yblock = block / num_blocks;
        
        int xstart = xblock * block_size;
        int ystart = yblock * block_size;
        
        // As `size` does not have to be a multiple of `block_size`, the last block might be smaller in each direction
        int max_x = xstart + block_size - 1;
        if(max_x >= size) {
            max_x = size - 1;
        }
        int max_y = ystart + block_size - 1;
        if(max_y >= size) {
            max_y = size - 1;
        }
        
        // Check matrix entry for each of the entries in the block
        int i, j;
        for(i = xstart; i <= max_x; ++i)
        {
            for(j = ystart; j <= max_y; ++j)
            {
                if(!check_matrix_entry(lu, matrix, size, i, j))
                    result = 0;
            }
        }
        
        
    }
	return result;
}