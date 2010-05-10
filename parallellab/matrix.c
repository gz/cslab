/* 
 * matrix.c - Matrix computations that should be optimized by the students
 * 
 * Copyright (c) 2007-2010 ETH Zurich, All rights reserved
 * Author: Mathias Payer <mathias.payer@inf.ethz.ch>, 
 * 
 * Only change these two functions and try to make them as fast as possible
 * on the given multi-core machine.
 *
 * Include here a description of your optimizations and the members
 * names and email addresses of your team.
 *
 * Provide some speedup measurements and briefly discuss them. Specify
 * the hostname of the computer that you used to obtain these results
 * (the output of 'hostname --fqdn', e.g., rifpc22.inf.ethz.ch).
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/** 
 * Decomposes a matrix in-situ into its L and U components
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 */
void decompose_matrix(double *matrix, int size)
{
  /* in situ LU decomposition */
  /* use parallelism */
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
