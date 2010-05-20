/* LU Decomposition:
 * ====================
 * - general description/optimization
 * - some speedup measurements
 * - hostname used to obtain these results hostname --fqdn
 *
 * Authors:
 * ====================
 * team07:
 * Boris Bluntschli (borisb@student.ethz.ch)
 * Gerd Zellweger (zgerd@student.ethz.ch)
 *
 */

// <config>

#define ENABLE_PARALLELIZATION
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
        #pragma omp parallel for
		for(j=k+1; j < size; j++) {
			matrix[index(k, j)] = matrix[index(k, j)] / divisor;
		}

        // (k+1, k+1) to (size-1, size-1)
		#pragma omp parallel for schedule(static, 64)
		for(i=k+1; i<size; i++) {
			int j;
			for(j=k+1; j<size; j++) {
				matrix[index(i, j)] = matrix[index(i, j)] - matrix[index(i, k)] * matrix[index(k, j)];
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

	int k;
	for(k = 0; k <= min && k != j; ++k) {
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
