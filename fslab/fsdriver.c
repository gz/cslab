/* FAT12 Driver
 * ====================
 * This is a simple FAT12 driver implementation. We have
 * functions for opening, closing, creating and reading and writing
 * to files. On fs_open a corresponding `file_table_entry` struct will
 * be created to keep track of the associated information for the file.
 * On the first fs_read call the buffer in the file_table_entry is loaded
 * with the file contents and the bytes we want to read are copied in the
 * client buffer.
 * On a fs_write call we overwrite the buffer and write it to disk (including
 * updating the corresponding `directory_entry`).
 * Calls to fs_creat create a new `directory_entry` in the corresponding directory.
 *
 * Known Limitations
 * ====================
 * - The code does not handle long file names.
 * - Code can not create directories.
 * - The path length is limited to 255 characters since strtok cannot handle const char* directly
 * - The code assumes that every non-root directory only has one cluster.
 *   This limits the number of files per directory to sizeof(dos_dir_entry) / cluster_size.
 * - A directory entry whose name starts with byte 0x0 is available and marks the end
 *   of the corresponding directory table.
 * - We do not update file access and creation dates and times of directory entries.
 *
 * Authors
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
typedef struct dos_dir_entry* directory_entry_ptr;

// internal file handle representation
struct file_table_entry {
	int pos;
	int buffer_size;
	data_ptr buffer;
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
#define min(x, y) ((x) < (y) ? (x) : (y))
#define IS_DIRECTORY(entry) ( ((entry) != NULL) && ((entry)->attr & FILE_ATTR_DIRECTORY) )
#define IS_FILE(entry)      ( ((entry) != NULL) && !((entry)->attr & FILE_ATTR_DIRECTORY) )
#define IS_LAST_CLUSTER(c)  ( (c) >= 0xFF7 )
#define IS_ODD_NUMBER(n)    ( (n) & 0x1 )
#define BIOS_READ_WRITE_SIZE 512 		// in bytes
#define MAX_FILENAME_LENGTH   13  		// filename (8 bytes) + dot (1byte) + extension (3 bytes)
#define MAX_PATH_LENGTH      255

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



/** Loads cluster data identified by `number` into `buffer`.
 *  @param number cluster to load
 *  @param buffer to write contents in
 */
static void load_cluster(uint number, data_ptr buffer) {

	// internally we work with cluster numbers from 0 to n-2 to calculate the offset
	number = number - 2;
	int cluster_start_sector = (root_dir_start_sector + root_dir_sectors) + (number * fbs.sec_per_clus);

	int i;
	for(i=0; i<fbs.sec_per_clus; i++) {
		bios_read(cluster_start_sector+i, buffer + (fbs.sector_size*i));
	}

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
	// Note the allocation size of at least cluster size is a bit of a hack here
	// since we can then use the same buffer in `get_file_handle` for other directories
	data_ptr root_dir_data = malloc( max(root_dir_sectors*fbs.sector_size, cluster_size) );

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
static file_handle create_file_handle(directory_entry_ptr entry) {

	file_handle fh = malloc( sizeof(struct file_table_entry) );
	fh->pos = 0;
	fh->buffer = NULL;

	memcpy(&fh->directory_entry, (data_ptr)entry, sizeof(struct dos_dir_entry));

	//_load_file_data(fh);
	return fh;
}


/** Pads `filename` with spaces "FILE" becomes -> "FILE    " and
 *  stores it in `fatname`.
 *  Note: This could probably be done easier?
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
	if(pch)
		memcpy(ext, filename+len-(pch_len-1), pch_len-1);

	sprintf(fatname, "%.8s.%.3s", name, ext);
}


/** Finds the entry with name `entry_name` in the corresponding directory_data.
 * @param directory_data contents of the directory
 * @param entry_name name to search for
 * @return pointer to the directory entry or NULL if no matching entry was found
 */
static directory_entry_ptr get_directory_entry(data_ptr directory_data, const char* search_entry_name) {
	DEBUG_PRINT("get entry %s\n", search_entry_name);

	char fat_search_name[MAX_FILENAME_LENGTH];
	convert_filename(search_entry_name, fat_search_name); // this is the name we have to search for in entries

	directory_entry_ptr current_entry = (directory_entry_ptr) directory_data;
	while(current_entry->name[0] != 0x0) {

		DEBUG_PRINT("reading directory entry: %s\n", current_entry->name);
		// ignore empty entries & long file names
		if( (*((data_ptr) current_entry) == 0xE5) && (current_entry->attr == 0x0F))
			continue;

		// generate entry name
		char current_entry_name[MAX_FILENAME_LENGTH];
		sprintf(current_entry_name, "%.8s.%.3s", current_entry->name, current_entry->ext);

		if(strcmp(current_entry_name, fat_search_name) == 0) {
			DEBUG_PRINT("found match on %s\n", current_entry->name);
			return current_entry;
		}

		current_entry++;
	}

	return NULL; // no matching entry found
}


/** This loads the corresponding file handle for a given path.
 * @param p path identifying the file
 * @return file handle for p or NULL if path is invalid.
 */
static file_handle get_file_handle(const char *p) {

	file_handle fh = NULL; // will be set with the file if we find it

	char path[MAX_PATH_LENGTH];
	strcpy(path, p);

	// Now we walk down the directory tree until we find the file we need
	data_ptr current_directory_data = load_root_directory();
	char* current_name_token;
	current_name_token = strtok(path, "/");
	directory_entry_ptr current_entry = NULL; // this is the directory entry which corresponds to the current_name_token
	boolean searching = TRUE;
	do {
		DEBUG_PRINT("searching for: %s\n", current_name_token);
		current_entry = get_directory_entry(current_directory_data, current_name_token);

		if( IS_FILE(current_entry) && (strtok(NULL, "/") == NULL) ) {
			// we have found the file we're searching for
			fh = create_file_handle(current_entry);
			searching = FALSE;
			break;
		}
		else if(IS_DIRECTORY( current_entry )) {
			current_name_token = strtok(NULL, "/");
			if(current_name_token != NULL) {
				// this only works since we assume directory size does never exceed 1 cluster
				load_cluster(current_entry->start, current_directory_data);
				continue;
			}
			else {
				searching = FALSE; // our path ends with a directory
			}
		}

		searching = FALSE; // if we come here something went wrong

	} while( searching );


	free(current_directory_data);
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


/** Closes a file. Frees resources in the file_table and the internal buffer.
 *	This function assumes that fd is a valid file descriptor.
 *	@param fd file descriptor previously handed out to clients by fs_open.
 */
void fs_close(int fd) {
	assert(fd >= 0 && fd < MAX_FILES);

	if(file_table[fd] != NULL) {
		file_handle fh = file_table[fd];

		if(fh->buffer != NULL)
			free(fh->buffer);

		free(fh);
		file_table[fd] = NULL;
	}
}


/** Gets the number of the next cluster for `active_cluster`.
 *  We work only with FAT1 here.
 *  FAT12 means we have 12 bits per cluster number which
 *  really means that in 3 bytes (24 bits) we have 2 cluster
 *  numbers stored. So we have to multiply our active cluster
 *  by 1.5 to get the correct offset in the fat table, and do
 *  some bit shifting to get the correct number in the end.
 *  @param cluster_nr number of the current cluster
 */
static int get_next_cluster_nr(int cluster_nr) {
	int fat_offset = cluster_nr + (cluster_nr / 2); // multiply by 1.5 [3 bytes per 2 cluster]

	unsigned short next_cluster_nr = *(unsigned short*)&FAT1[fat_offset];

	if(IS_ODD_NUMBER(cluster_nr)) {
		next_cluster_nr = next_cluster_nr >> 4;
	}
	else {
		next_cluster_nr = next_cluster_nr & 0x0FFF;
	}

	return next_cluster_nr;
}


/** Loads the contents of a file from disk into the file handle buffer.
 *  If the buffer is non null then the function will free the buffer
 *  and replace it with the newly allocated one.
 *  Loading a file works by walking through the cluster chain
 *  using get_next_cluster. The buffer size is expanded as long as we
 *  have more clusters to load. Note that realloc with a null pointer
 *  is equivalent to calling malloc.
 *  @param file handle to load content for
 */
static void load_file_contents(file_handle fh) {

	if(fh->buffer != NULL) {
		free(fh->buffer);
		fh->buffer = NULL;
	}

	int current_cluster_nr = fh->directory_entry.start;
	int i = 0;
	while(!IS_LAST_CLUSTER(current_cluster_nr)) {

		fh->buffer = realloc(fh->buffer, cluster_size*(i+1));

		char cluster_data[cluster_size];
		load_cluster(current_cluster_nr, cluster_data);
		memcpy(fh->buffer+(i++*cluster_size), cluster_data, cluster_size);

		current_cluster_nr = get_next_cluster_nr(current_cluster_nr);

	}

}


/** Reads `len` bytes from a file into the `buffer`.
 *  This function loads the content of a file from disk to memory (fh->buffer)
 *  if not already done before.
 *  The limitation here is that we load the whole file no matter how big it is or
 *  how much we really read from it.
 *	@param fd file descriptor identifying the file in the file_table
 *	@param buffer to write to
 *	@param len number of bytes to read
 *	@return number of bytes read (can be less than `len` if file size - current seek position
 *			is less than len)
 */
int fs_read(int fd, void *buffer, int len) {
	assert(fd >= 0 && fd < MAX_FILES);

	file_handle fh = file_table[fd];

	if(fh != NULL) {

		// lazy loading file contents on first read
		if(fh->buffer == NULL)
			load_file_contents(fh);

		// make sure we don't read more than we can
		int bytes_to_read = min(len, fh->directory_entry.size - fh->pos);
		memcpy(buffer, fh->buffer+fh->pos, bytes_to_read);

		fh->pos += bytes_to_read; // update position in file handle
		return bytes_to_read;
	}

	return -1; // invalid file descriptor
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
