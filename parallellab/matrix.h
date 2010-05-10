/* 
 * matrix.h - Header file for parallel with functions that should be optimized
 *            by the students
 * 
 * Copyright (c) 2007 ETH Zurich, All rights reserved
 * Author: Mathias Payer <mathias.payer@inf.ethz.ch>, 
 * 
 * May not be used, modified, or copied without permission.
 */

/** Decomposes a matrix in-situ into its L and U components
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 */
void decompose_matrix(double *matrix, int size);

/** Checks if the computation of l*u is the same as matrix
 * @param lu     pointer to the lu matrix
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 * @return       true if l*u=matrix, false otherwise
 */
int check_matrix(double *lu, double *matrix, int size);
