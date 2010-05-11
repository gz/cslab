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


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/** 
 * Decomposes a matrix in-situ into its L and U components
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 */
void decompose_matrix(double* matrix, int size) {

	int k, j, i;
	for(k=0; k < size; k++) {

		for(j=k+1; j < size; j++) {
			matrix[k*size+j] = matrix[k*size+j] / matrix[k*size+k];
		}

		for(i=k+1; i<size; i++) {
			for(j=k+1; j<size; j++) {
				matrix[i*size+j] = matrix[i*size+j] - matrix[i*size+k] * matrix[k*size+j];
			}
		}

	}

}

/** 
 * Checks if the computation of l*u is the same as matrix
 * @param lu     pointer to the lu matrix
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 * @return       true if l*u=matrix, false otherwise
 */
int check_matrix(double *lu, double *matrix, int size)
{
	/* return l*u == matrix */
	/* use parallelism! */

	return  1;
}
