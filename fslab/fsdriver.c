/* FAT12 Driver
 * ====================
 *
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
#include <assert.h>
#include "fs.h"

// Some basic types & macros
typedef int boolean;
typedef unsigned char* data_ptr;
typedef struct dos_dir_entry* directory_entry;
#define TRUE 1
#define FALSE 0
#define max(x, y) ((x) > (y) ? (x) : (y))

// Macro for Debugging
#define DEBUG 1
#ifdef DEBUG
	#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
	#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release */
#endif

// Macros to read out bytes at some memory address, this is used to initialize
// structs with values read from the disk
#define GET_FOUR_BYTES(ptr) ((__u32) (*(__u32*) (ptr)))
#define GET_TWO_BYTES(ptr)  ((__u16) (*(__u16*) (ptr)))
#define GET_ONE_BYTE(ptr)   ((__u8 ) (*(__u8 *) (ptr)))

// Global Definitions & Variables
#define BIOS_READ_WRITE_SIZE 512 // in bytes

static int root_dir_start_sector = 0; // first sector of the root directory
static int root_dir_sectors = 0; // # of sectors reserved for root directory
static int cluster_size = 0; // in bytes
static int next_file_slot = 0; // determines where in the file table we currently are
static int fat_size = 0; // in bytes



/** Loads data of FAT{1,2,3...}.
 *  @param which tells the function to load FAT1 (which = 1) or FAT2 (which = 2) etc.
 *  @return allocated chunk of memory containing the FAT table.
 *  note: this function works for an arbitrarily number of FATs but
 *  our images have 2 in general.
 */
static data_ptr load_fat(int which) {
	assert(fbs.fats >= which); // FAT must exist

	data_ptr fat = malloc(fat_size);

	// determine which fat to load (FAT1 is at offset fbs.reserved)
	int fat_index = which-1;
	int fat_start_sector = fbs.reserved + fat_index*fbs.fat_length;

	int i;
	for(i=0; i < fbs.fat_length; i++) {
		bios_read(fat_start_sector+i, fat+i*fbs.sector_size);
	}

	return fat;
}


/** Initialization at the beginning. This reads out the
 *  first sector in our disk and initializes the fbs (First Boot
 *  Sector Struct).
 */
void fs_init() {
	char boot_sector[BIOS_READ_WRITE_SIZE];
	bios_read(0, boot_sector);

	// set ignored (3 bytes)
	memcpy(fbs.ignored, boot_sector, 3);

	// set system id (8 bytes)
	memcpy(fbs.system_id, boot_sector+3, 8);

	// Initialize rest of First Boot Sector
	fbs.sector_size  = GET_TWO_BYTES(boot_sector+11);
	fbs.sec_per_clus = GET_ONE_BYTE(boot_sector+13);
	fbs.reserved     = GET_TWO_BYTES(boot_sector+14);
	fbs.fats         = GET_ONE_BYTE(boot_sector+16);
	fbs.dir_entries  = GET_TWO_BYTES(boot_sector+17);
	fbs.sectors      = GET_TWO_BYTES(boot_sector+19);
	fbs.media        = GET_ONE_BYTE(boot_sector+21);
	fbs.fat_length   = GET_TWO_BYTES(boot_sector+22);
	fbs.secs_track   = GET_TWO_BYTES(boot_sector+24);
	fbs.heads        = GET_TWO_BYTES(boot_sector+26);
	fbs.hidden       = GET_FOUR_BYTES(boot_sector+28);
	fbs.total_sect   = GET_FOUR_BYTES(boot_sector+32);

	// This code makes the assumption that bios_read (reads 512 bytes)
	// corresponds to exactly 1 FAT12 sector.
	assert(fbs.sector_size == BIOS_READ_WRITE_SIZE);

	// Initializing global variables
	cluster_size = fbs.sector_size * fbs.sec_per_clus;
	root_dir_start_sector = (fbs.reserved + (fbs.fats * fbs.fat_length));
	root_dir_sectors = (fbs.dir_entries * sizeof(struct dos_dir_entry)) / fbs.sector_size;
	fat_size = fbs.fat_length*fbs.sector_size;

	assert(fbs.fats >= 1); // we need at least 1 FAT
	FAT1 = load_fat(1);
	//FAT2 = load_fat(2);

	// Print some information useful for debugging
	DEBUG_PRINT("system id: %.8s\n", fbs.system_id);
	DEBUG_PRINT("sector size: %d\n", fbs.sector_size);
	DEBUG_PRINT("fat table count: %d\n", fbs.fats);
	DEBUG_PRINT("fat length: %d\n", fbs.fat_length);
	DEBUG_PRINT("sector count: %d\n", fbs.sectors);
	DEBUG_PRINT("root dir entrys: %d\n", fbs.dir_entries);
	DEBUG_PRINT("sectors per cluster: %d\n", fbs.sec_per_clus);
	DEBUG_PRINT("root dir start sector: %d\n", root_dir_start_sector);
	DEBUG_PRINT("root dir sector length: %d\n", root_dir_sectors);

}

/* opens a file */
int fs_open(const char *p)
{
  return -1;
}

/* closes a file */
void fs_close(int fd)
{
}

/* reads from a file len bytes into the buffer */
int fs_read(int fd, void *buffer, int len)
{
  return -1;
}

/* creates a file */
int fs_creat(const char *p)
{
  /* consider the dir structure already there */
  return -1;
}


/* writes a file */
int fs_write(int fd, void *buffer, int len)
{
  return -1;
}
