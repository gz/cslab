/**
 * @file   fstest.c
 * @author Cristian Tuduce
 * @author Matteo Corti
 * @brief  simple tool to test the FAT driver
 */

#include "fs.h"
#include <stdio.h>

/** @def BUFFER_SIZE
 * size of a read/write buffer
 */
#define BUFFER_SIZE 512

/**
 * Prints an error on the test usage end exits
 */
static void usage() {
  fprintf(stderr, "Usage: ./testfs image\n");
  exit(EXIT_FAILURE);
}

/**
 * main routine: starts the tests
 * @param argc number of arguments
 * @param argv command line arguments
 * @return     0 if no errors occurred
 */
int main(int argc, char **argv) {

  FILE  * commandsfd    = NULL;
  char  * buffer        = NULL;
  char  * command_file  = NULL;
  char  * line          = NULL;
  char    command;
  int     bytes         = 0;
  int     count         = 0;
  int     fds[10];
  int     id            = 0;
  int     read_bytes    = 0;
  int     written_bytes = 0;
  size_t  len           = 0;
  ssize_t read          = 0;

  /* we should get one command line argument: *
   * the image file name                      */
  if (argc != 2) {
    usage();
  }

  printf("Testing image: %s\n", argv[1]);

  /* initialize the disk driver with the image name */
  bios_init(argv[1]);

  /* open the command file (i.e., image_name.command) */
  
  command_file = malloc(strlen(argv[1]) + 9);
  if (!command_file) {
    fprintf(stderr, "Error: out of memory\n");
    exit(EXIT_FAILURE);
  }
  strcpy(command_file, argv[1]);
  strcat(command_file, ".command");
  if (!(commandsfd = fopen(command_file, "r"))) {
    fprintf(stderr, "Error: Cannot open %s\n", command_file);
    exit(EXIT_FAILURE);
  }

  /* we allocate the r/w buffer with one more byte to null terminate the *
   * string and ensure that printf will not read after the buffer        */
  buffer = malloc(BUFFER_SIZE + 1);

  /* execute the filesystem initialization */
  fs_init();
  
  /* read the commands */
  while ((read = getline(&line, &len, commandsfd)) != -1) {

    /* parse the command line:
     * - 1 char: command
     * - 1 char: id
     * - space
     * - file name
     */

    /* 0-terminate the line */
    line[read - 1] = '\0';

    /* skip comments and lines not beginning with a command */
    if (line[0] == '#'  ||
        line[0] == ' '  ||
        line[0] == '\0' ||
        (line[0] == '/' && read > 1 && line[1] == '/')) {
      continue;
    }
    
    command        = line[0];
    id             = strtol(line+1, (char **)NULL, 10);
    if (id <= 0) {
      fprintf(stderr, "Error: wrong file descriptor %c\n", *(line+1));
      exit(EXIT_FAILURE);
    }
    if (id >= 10) {
      fprintf(stderr, "Error: wrong file descriptor %i (should be < 10)\n", id);
      exit(EXIT_FAILURE);
    }
    
    line           = line + 3;

    switch (command) {
    case 'c':
      printf("Closing file %i\n", id);
      fs_close(fds[id]);
      break;
    case 'n':
      printf("Creating file %i %s\n", id, line);
      fds[id] = fs_creat(line);
      if (fds[id] == -1) {
        fprintf(stderr, "Error: file %s exists\n", line);
        exit(EXIT_FAILURE);
      }
      break;
    case 'o':
      printf("Opening file %i %s\n", id, line);
      if ((fds[id] = fs_open(line)) == -1) {
	fprintf(stderr, "Error: fs_open(%s) failed!\n", line);
	exit(EXIT_FAILURE);
      }
      break;
    case 'r':
      bytes = atoi(line);
      printf("Reading %i bytes from file %i\n", bytes, id);
      count = bytes;
      while (count > 0) {
	read_bytes = fs_read(fds[id], buffer, 512);
	if (read_bytes == 0) {
          // EOF
	  break;
	}
	buffer[read_bytes] = '\0';
	printf("%s", buffer);
	count -= read_bytes;
      }
      break;
    case 'w':
      len = strlen(line);
      printf("Writing %i bytes to file %i\n", (int)len , id);
      /* if the buffer is not large enough enlarge it */
      if (len > BUFFER_SIZE) {
        buffer = realloc(buffer, len+1);
      }
      strcpy(buffer, line);
      written_bytes = fs_write(fds[id], buffer, (int)strlen(line));
      if (written_bytes < (int)strlen(line)) {
	fprintf(stderr, "Error: fs_write failed!\n");
	exit(EXIT_FAILURE);
      }
      break;
    default:
      fprintf(stderr, "Error: unknown command '%c'\n", command);
      exit(EXIT_FAILURE);
    }
    
    line -= 3;
    
  }

  printf("Test finished\n");

  /* close the disk image */
  bios_shutdown();

  /* free memory */
  if (line) {
    free(line);
  }
  free(FAT1);
  free(FAT2);
  free(buffer);
  free(command_file);

  if (fclose(commandsfd) != 0) {
    fprintf(stderr, "Error: Cannot close %s\n", command_file);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);

}
