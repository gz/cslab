/* FAT12 Driver
 * ====================
 *
 * Known Limitations
 * ====================
 * - The code does not handle long file names
 * - The path length is limited to 255 characters since strtok cannot handle const char* directly
 * - The code assumes that every non-root directory only has one cluster.
 *   This limits the number of files per directory to sizeof(dos_dir_entry) / cluster_size.
 * - A directory entry whose name starts with byte 0x0 is available and marks the end
 *   of the corresponding directory table.
 * - We do not update file access and creation dates and times of directory entries.
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
#define TRUE 1
#define FALSE 0
typedef unsigned int uint;
typedef unsigned char* data_ptr;

// Types needed for our FAT Implementation
typedef struct dos_dir_entry* directory_entry;

// internal file handle representation
struct file_table_entry {
	int pos;
	char* buffer;
	int dir_cluster_nr; // this is the cluster where the corresponding file entry for this file is
						// if dir_cluster_nr == 0: then, this file is in the root dir
	struct dos_dir_entry directory_entry;
};
typedef struct file_table_entry* file_handle;


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
#define max(x, y) ((x) > (y) ? (x) : (y))
#define IS_DIRECTORY(entry) ( ((entry) != NULL) && ((entry)->attr & FILE_ATTR_DIRECTORY) )
#define IS_FILE(entry) ( ((entry) != NULL) && !((entry)->attr & FILE_ATTR_DIRECTORY) )
#define BIOS_READ_WRITE_SIZE 512 		// in bytes
#define MAX_FILENAME_LENGTH 13  		// filename (8 bytes) + dot (1byte) + extension (3 bytes)
#define MAX_PATH_LENGTH 255

static int root_dir_start_sector = 0; 	// first sector of the root directory
static int root_dir_sectors = 0;		// # of sectors reserved for root directory
static int cluster_size = 0; 			// in bytes
static int fat_size = 0; 				// in bytes


/** Writes data at new_fat in FAT`which`.
 * @param which FAT to write to.
 * @param new_fat new FAT table data
 */
static void write_fat(uint which, data_ptr new_fat) {
	assert(fbs.fats >= which); // FAT must exist

	// determine which fat to write (FAT1 is at offset fbs.reserved)
	int fat_index = which-1;
	int fat_start_sector = fbs.reserved + fat_index*fbs.fat_length;

	int sector;
	for(sector=0; sector<fbs.fat_length; sector++) {
		bios_write(fat_start_sector+sector, new_fat + sector*fbs.sector_size);
	}

}


/** This is just a helper function which writes new_fat in all existing FATs.
 *  @param new_fat data to write in all FATs.
 */
static void write_all_fats(data_ptr new_fat) {
	int fat_nr;
	for(fat_nr=1; fat_nr <= fbs.fats; fat_nr++)
		write_fat(fat_nr, new_fat);
}



/** Loads data of FAT{1,2,3...}.
 *  @param which tells the function to load FAT1 (which = 1) or FAT2 (which = 2) etc.
 *  @return allocated chunk of memory containing the FAT table.
 *  note: this function works for an arbitrarily number of FATs but
 *  our images have 2 in general.
 */
static data_ptr load_fat(uint which) {
	assert(fbs.fats >= which); // FAT must exist

	data_ptr fat = malloc(fat_size);

	// determine which fat to load (FAT1 is at offset fbs.reserved)
	int fat_index = which-1;
	int fat_start_sector = fbs.reserved + fat_index*fbs.fat_length;

	int sector;
	for(sector=0; sector<fbs.fat_length; sector++) {
		bios_read(fat_start_sector+sector, fat + sector*fbs.sector_size);
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

	// This code makes the assumption that bios_read/bios_write units
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

	// Clear the file table
	int i;
	for(i=0; i< MAX_FILES; i++) {
		file_table[i] == NULL;
	}

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


/** Finds first free entry in file table.
 *  @return This function returns the index of the first
 *  free entry found or -1 if the file table is currently full.
 */
static int find_free_file_slot() {

	int i;
	for(i=0; i< MAX_FILES; i++) {
		if(file_table[i] == NULL)
			return i;
	}

	return -1; // file table is full
}


/** Loads the root directory and returns its content.
 *  @return contents of the root directory
 */
static data_ptr load_root_directory() {
	data_ptr root_dir_data = malloc(root_dir_sectors*fbs.sector_size);

	int i;
	for(i=0; i<root_dir_sectors; i++) {
		char sector_data[fbs.sector_size]; // buffer for current file sector
		bios_read(root_dir_start_sector+i, sector_data);

		memcpy(root_dir_data+(i*fbs.sector_size), sector_data, fbs.sector_size);
	}

	return root_dir_data;
}

/** Saves the root directory to disk.
 * @param root_directory content of the root directory
 */
static void save_root_directory(data_ptr root_directory) {

	int i;
	for(i=0; i<root_dir_sectors; i++) {
		char root_dir_sector[fbs.sector_size]; // buffer for current file sector
		memcpy(root_dir_sector, root_directory + (i*fbs.sector_size), fbs.sector_size);

		bios_write(root_dir_start_sector+i, root_dir_sector);
	}

}

/** Allocates and initializes a file handle for a given directory entry.
 * @param entry the corresponding directory entry
 * @return A file handle for the file.
 */
static file_handle create_file_handle(directory_entry entry) {

	file_handle fh = malloc( sizeof(struct file_table_entry) );
	fh->pos = 0;
	fh->buffer = NULL;

	memcpy(&fh->directory_entry, (data_ptr)entry, sizeof(struct dos_dir_entry));

	//_load_file_data(fh);
	return fh;
}


/** Pads `filename` with spaces "FILE" becomes -> "FILE    " and
 *  stores it in `fatname`.
 *  This assumes that filename is not longer than MAX_FILENAME_LENGTH
 *  @param filename filename to transform
 *  @param fatname transformed name, conforms to name in directory entries.
 */
static void convert_filename(const char* filename, char* fatname) {
	assert(strlen(filename) <= MAX_FILENAME_LENGTH);

	int len = strlen(filename);
	char *pch = strrchr(filename, '.');

	char ext[3]  = {' ', ' ', ' '};
	char name[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

	int pch_len = (pch == NULL) ? 0 : strlen(pch);
	memcpy(name, filename, len-pch_len);
	memcpy(ext, filename+len-pch_len, pch_len);

	sprintf(fatname, "%.8s.%.3s", name, ext);
}


/** Finds the entry with name `entry_name` in the corresponding directory_data.
 * @param directory_data contents of the directory
 * @param entry_name name to search for
 * @return pointer to the directory entry or NULL if no matching entry was found
 */
static directory_entry get_directory_entry(data_ptr directory_data, const char* search_entry_name) {

	char fat_search_name[MAX_FILENAME_LENGTH];
	convert_filename(search_entry_name, fat_search_name); // this is the name we have to search for in entries

	directory_entry current_entry = (directory_entry) directory_data;
	while(current_entry->name[0] != 0x0) {

		DEBUG_PRINT("reading directory entry: %s\n", current_entry->name);
		// ignore empty entries & long file names
		if( (*((data_ptr) current_entry) == 0xE5) && (current_entry->attr == 0x0F))
			continue;

		// generate entry name
		char current_entry_name[MAX_FILENAME_LENGTH];
		sprintf(current_entry_name, "%.8s.%.3s", current_entry->name, current_entry->ext);

		if(strcmp(current_entry_name, fat_search_name) == 0) {
			return current_entry;
		}

		current_entry++;
	}

	return NULL; // matching entry not found
}


/** This loads the corresponding file handle for a given path.
 * @param p path identifying the file
 * @return file handle for p or NULL if path is invalid.
 */
static file_handle get_file_handle(const char *p) {

	file_handle fh = NULL; // will be set with the file if we find it
	data_ptr root_directory = load_root_directory();

	char path[MAX_PATH_LENGTH];
	strcpy(path, p);

	// Now we walk down the directory tree until we find the file we need
	data_ptr current_directory_data = root_directory;
	char* current_name_token;
	current_name_token = strtok(path, "/");
	directory_entry current_entry = NULL; // this is the directory entry which corresponds to the current_name_token

	do {
		current_entry = get_directory_entry(current_directory_data, current_name_token);

		if( IS_FILE(current_entry) ) {

			// TODO: check that (strtok(NULL, "/") == NULL
			// we have found the file we're searching for
			fh = create_file_handle(current_entry);
			break;

		}
		else if(IS_DIRECTORY( current_entry )) {
			current_name_token = strtok(NULL, "/");
			if(current_name_token != NULL) {
				DEBUG_PRINT("TODO: find file out of the root dir.");
				break;
			}
		}

	} while( IS_DIRECTORY(current_entry) );


	free(root_directory);
	return fh;
}


/** Opens a file located at path p.
 *  @param string containing the path to the file to load.
 *  @return file descriptor which identifies the file for read/write/close operations.
 *  or -1 if the given path is invalid.
 */
int fs_open(const char *p) {
	int fd;

	if( (fd = find_free_file_slot()) == -1 )
		return -1; // currently more than MAX_FILES open

	file_handle fh = get_file_handle(p);

	if(fh != NULL) {
		file_table[fd] = fh;
		return fd;
	}
	else {
		return -1; // file not found
	}

}

/* closes a file */
void fs_close(int fd) {

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
