/**
 * @file   bios.c
 * @author Cristian Tuduce
 * @author Matteo Corti
 * @brief  a simple disk driver working on disk images
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "fs.h"

/** @def SECTOR_SIZE
 * Size of a sector
 */
#define SECTOR_SIZE 512

static int fd = 0; /**< file descriptor */

/** Initialize the disk driver
 * @param name disk image file name
 */
void bios_init(char *name) {
/*   printf(">>> bios_init(%s)\n", name); */
  fd = open(name, O_RDWR);
  if (fd == -1) {
    printf("Error: cannot open disk image (%s)\n", name);
    exit(EXIT_FAILURE);
  }
  memset(file_table, 0, sizeof(file_table));
}

/** Unmounts the disk image
 */
void bios_shutdown() {
/*   printf(">>> bios_shutdown()\n"); */
  if (fd > 0) {
    if (close(fd) == -1) {
      printf("Error: cannot close disk image\n");
      exit(EXIT_FAILURE);
    }
  }
}

/** Read a disk sector
 * @param number sector number
 * @param sector where to write the data
 */
void bios_read(int number, char *sector) {

/*   printf(">>> bios_read(%i, sector)\n", number); */

  ssize_t count;

  if (lseek(fd, (off_t) (number * SECTOR_SIZE), SEEK_SET) !=
      (off_t) number * SECTOR_SIZE) {
    printf("Cannot access sector %i\n", number);
    exit(EXIT_FAILURE);
  }
  if ((count = read(fd, sector, SECTOR_SIZE)) < SECTOR_SIZE) {
    printf("Error reading sector %i (read %i bytes)\n",
	    number, (int)count);
    exit(EXIT_FAILURE);
  }
}

/** Write a disk sector
 * @param number sector number
 * @param sector data to write
 */
void bios_write(int number, char *sector) {

/*   printf(">>> bios_write(%i, sector)\n", number); */

  if (!lseek(fd, (off_t) (number * SECTOR_SIZE), SEEK_SET)) {
    printf("Cannot access sector %i\n", number);
    exit(EXIT_FAILURE);
  }
  if (write(fd, sector, SECTOR_SIZE) < SECTOR_SIZE) {
    printf("Error reading sector %i\n", number);
    exit(EXIT_FAILURE);
  }
}
