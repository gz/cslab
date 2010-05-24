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

* Speedup measurements were done on a Q6660 @ 2.40 GHz (QuadCore)
 

==================
DECOMPOSITION
==================

decompose (1000 x 1000)
------------------------------
1 thread:  1.01, 0.98, 0.98
2 threads: 0.39, 0.49, 0.35
4 threads: 0.45, 0.30, 0.53

decompose (2000 x 2000)
------------------------------
1 thread:  11.03, 10.94
2 threads: 11.42
4 threads: 


=================
CHECKING
=================

checking (1000 x 1000)
----------------------
           normal                blocked
1 thread:  1.89  1.8   1.84      1.65  1.65  1.66
2 threads: 0.9   0.94  0.97      0.85  0.85  0.87
4 threads: 0.49  0.54  0.68      0.49  0.47  0.46

checking (2000 x 2000)
----------------------
           normal           blocked
1 thread:  18.56 18.48      17.35 17.57
2 threads: 9.73  10.05      8.85  8.86
4 threads: 5.48  5.68       4.85  4.91

For checking, we achieve an almost linear speedup. Instead of parallelizing one of the loops (outer or inner), we decided to 
use striping and run an index over all the fields of the matrix that we have to check. This seemed to yield better results in the
settings that we tested. We also implemented a blocked implementation (check_matrix_blocked), which yielded slightly better results.

If we look at the implementation of both methods, they are both *theoretically* completely parallelized, but of course there are problems
as there exists contention for the cache.

 */

// <config>

#define NUM_THREADS 4

// </config>
 
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define index(i, j) ((i)*size+(j))


void decompose_matrix(double* matrix, int size) {
    omp_set_num_threads(NUM_THREADS);

	int k, i;
    
	for(k=0; k < size; k++) {
		// (k, k+1) to (k, size-1)
        double pivot = matrix[index(k, k)];
        #pragma omp parallel for
        for(i=k+1; i < size; ++i) {
            matrix[index(k, i)] = matrix[index(k, i)] / pivot;        
        }
        
        // (k+1, k+1) to (size-1, size-1)
		#pragma omp parallel for
		for(i=k+1; i<size; i++) {
			int j;
			for(j = k + 1; j < size; j += 1) {
				matrix[index(i, j)] -= matrix[index(i, k)] * matrix[index(k, j)];
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
    printf("%f\n", diff);
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
        int pos;
        #pragma omp parallel for schedule(static, 1) private(pos)
        for(pos = 0; pos < size*size; ++pos)
        {
            int i = pos % size;
            int j = pos / size;
            if(!check_matrix_entry(lu, matrix, size, i, j))
                result = 0;
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