/* 
 * parallel.c - Main file for the parallel lab of the CS lab
 * 
 * Copyright (c) 2007 ETH Zurich, All rights reserved
 * Author: Mathias Payer <mathias.payer@inf.ethz.ch>,
 *         Oliver Trachsel <oliver.trachsel@inf.ethz.ch>
 * 
 * May not be used, modified, or copied without permission.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "matrix.h"
#include "parallel.h"

#define MIN_MATRIX_SIZE 5
#define MAX_MATRIX_SIZE 10000

static void fill_matrix(double *matrix, int size);
static void print_matrix(double *matrix, int size);
static long timediff_ms(struct timeval *x, struct timeval *y);


/** 
 * Main driver code for the parallel lab. Generates the matrix of the
 * specified size, initiates the decomposition and checking
 * routines. 
 */
int main (int argc, char *argv[]) 
{
  int size = 0;
  double *a = NULL;
  double *lu = NULL;
 
  clock_t start, time1, time2; 
  struct timeval start_timeval, end_timeval;
  double elapsed_secs, elapsed_total_secs, cpu_secs;

  /* Bail out if we don't have the correct number of parameters */
  if (argc!=2) {
    printf("This program is used to decompose a (random) matrix A into its components L and U.\n");
    printf("Usage: %s <matrix size>\n", argv[0]);
    return -1;
  }
  size = atoi(argv[1]);

  /* Adjust matrix size */
  if (size < MIN_MATRIX_SIZE) {
    printf("Setting matrix size to minimum value %d.\n", MIN_MATRIX_SIZE);
    size = MIN_MATRIX_SIZE;
  } else if (size > MAX_MATRIX_SIZE) {
    printf("Setting matrix size to maximum value %d.\n", MAX_MATRIX_SIZE);
    size = MAX_MATRIX_SIZE;
  }
  
  /* Generate data. */
  printf("LU matrix decomposition, starting warmup...\n");
  printf(" - Generating a %i * %i matrix\n", size, size);
  a = (double*)malloc(sizeof(double)*size*size);
  lu = (double*)malloc(sizeof(double)*size*size);
  if (a==NULL || lu==NULL) {
    printf("Not enough memory!\n");
    return -1;
  }
  fill_matrix(a, size);
  print_matrix(a, size);
  memcpy(lu, a, sizeof(double)*size*size);

  /* Start LU decomposition. */
  printf("Decomposing the matrix into its components...\n");
  gettimeofday(&start_timeval, NULL);
  start = clock();
  decompose_matrix(lu, size);
  time1 = clock()-start;
  gettimeofday(&end_timeval, NULL);
  elapsed_total_secs = elapsed_secs = (double)timediff_ms(&start_timeval, &end_timeval)/1000.0;
  cpu_secs = (double)(time1)/CLOCKS_PER_SEC;

  /* Verify resulting decomposition. */
  printf("Checking result...\n");
  print_matrix(lu, size);
  gettimeofday(&start_timeval, NULL);
  start = clock();
  if (check_matrix(lu, a, size))
    printf("The computation seems correct\n");
  else
    printf("The computation seems not correct\n");
  time2 = clock()-start;
  gettimeofday(&end_timeval, NULL);
  
  /* Output stats. */
  printf("\nDecomposition time: %.2fs CPU, %.2fs elapsed, %.1f%% speedup\n", 
	 cpu_secs, elapsed_secs, cpu_secs/elapsed_secs*100.0);
  elapsed_secs = (double)timediff_ms(&start_timeval, &end_timeval)/1000.0;
  elapsed_total_secs += elapsed_secs;
  cpu_secs = (double)(time2)/CLOCKS_PER_SEC;
  printf("Checking time:      %.2fs CPU, %.2fs elapsed, %.1f%% speedup\n", 
	 cpu_secs, elapsed_secs, cpu_secs/elapsed_secs*100.0);
  cpu_secs = (double)(time1+time2)/CLOCKS_PER_SEC;
  printf("Overall time:       %.2fs CPU, %.2fs elapsed, %.1f%% speedup\n", 
	 cpu_secs, elapsed_total_secs, cpu_secs/elapsed_total_secs*100.0);

  /* Free resources. */
  free(lu);
  free(a);
  return 0;
}


/** Fills a matrix with random values
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 */
static void fill_matrix(double *matrix, int size)
{
  int i,j;
  /* initialize pseudo random generator */
  srand(time (0));
  /* some random values... */
  for (i=0; i<size; i++)
    for (j=0; j<size; j++)
      matrix[i*size+j]=rand()/100;
}


/** 
 * Prints a given matrix
 * @param matrix pointer to the matrix
 * @param size   size of the matrix
 */
static void print_matrix(double *matrix, int size)
{
  int i,j;
  if (size>20) {
    printf("Too large to print... just doing the calculations...\n");
    return;
  }
  /* print that beast */
  for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++) {
      printf(" %10.2e", matrix[size*i+j]);
    }
    printf("\n");
  }
}


/** 
 * Computes and returns the time difference of two timevals in
 * milliseconds.
 */
static long timediff_ms (struct timeval *x, struct timeval *y)
{
  struct timeval result;

  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }
  
  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result.tv_sec = x->tv_sec - y->tv_sec;
  result.tv_usec = x->tv_usec - y->tv_usec;
  
  long diff_us =  result.tv_sec * 1000000 + result.tv_usec;
  if (x->tv_sec < y->tv_sec) {
    diff_us = -diff_us;
  }

  return diff_us / 1000;
}
