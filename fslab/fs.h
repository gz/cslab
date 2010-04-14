/**
 * @file   fs.h
 * @author Cristian Tuduce
 * @author Matteo Corti
 * @brief  a simple FAT filesystem driver: driver structures and functions
 */

#include <stdlib.h>
#include <string.h>
//#include "getline.h" // include for non-GNU platforms lacking the getline function in libc

/** @typedef unsigned char  __u8
 * 8 bit unsigned integer */
typedef unsigned char  __u8;

/** @typedef unsigned char  __u16
 * 16 bit unsigned integer */
typedef unsigned short __u16;

/** @typedef unsigned char  __u32
 * 32 bit unsigned integer */
typedef unsigned int   __u32;

/** Boot sector */
struct fat_boot_sector {

  /* common section */
  
  __u8  ignored[3];		/**< boot strap short or near jump */
  __u8  system_id[8];		/**< OEM name */
  __u16 sector_size;		/**< bytes per logical sector */
  __u8  sec_per_clus;		/**< sectors/cluster */
  __u16 reserved;		/**< reserved sectors */
  __u8  fats;			/**< number of FATs */
  __u16 dir_entries;		/**< root directory entries */
  __u16 sectors;		/**< number of sectors */
  __u8  media;			/**< media code */
  __u16 fat_length;		/**< sectors/FAT */
  __u16 secs_track;		/**< sectors per track */
  __u16 heads;			/**< number of heads */
  __u32 hidden;			/**< hidden sectors (unused) */
  __u32 total_sect;		/**< number of sectors (if sectors == 0) */

  /* extended BIOS parameter block (FAT12 & FAT16) */
  
  __u8  drive_number;           /**< physical drive number */
  __u8  cur_head;               /**< reserved (current head) */
  __u8  signature;              /**< signature */
  __u32 id;                     /**< serial number */
  __u8  volume[11];             /**< volume label */
  __u8  type[8];                /**< FAT file system type */
} __attribute__ ((packed));

struct fat_boot_sector fbs;     /**< boot sector */

unsigned char *FAT1; /**< first FAT */
unsigned char *FAT2; /**< second FAT */

/** DOS directory entry                                                  */
struct dos_dir_entry {
  __u8  name[8];                /**< name                                */
  __u8  ext[3];		        /**< extension                           */
  __u8  attr;			/**< attribute bits                      */
  __u8  lcase;			/**< case for base and extension         */
  __u8  ctime_cs;		/**< creation time, centiseconds (0-199) */
  __u16 ctime;			/**< creation time                       */
  __u16 cdate;			/**< creation date                       */
  __u16 adate;			/**< last access date                    */
  __u16 starthi;		/**< high 16 bits of cluster in FAT32    */
  __u16 time;			/**< last modified or created            */
  __u16 date;                   /**< date                                */
  __u16 start;			/**< first cluster of the file           */
  __u32 size;			/**< file size (in bytes)                */
} __attribute__ ((packed));

/** @def FILE_ATTR_RONLY
 * File is read only */
#define FILE_ATTR_RONLY     1

/** @def FILE_ATTR_HIDDEN
 * File is hidden */
#define FILE_ATTR_HIDDEN    2

/** @def FILE_ATTR_SYSTEM
 * System file */
#define FILE_ATTR_SYSTEM    4

/** @def FILE_ATTR_VOLUME
 * Volume label */
#define FILE_ATTR_VOLUME    8

/** @def FILE_ATTR_DIRECTORY
 * Directory */
#define FILE_ATTR_DIRECTORY 16

/** @def FILE_ATTR_ARCHIVE
 * File is an archive */
#define FILE_ATTR_ARCHIVE   32

/** @def MAX_FILES
 * The maximum number of files that can be open at a time */
#define MAX_FILES 4

void *file_table[MAX_FILES]; /**< File table: calls to open will
                              * populate this table and return an
                              * index in the table. The entries in the
                              * table are pointers to structures that
                              * keep info about the opened file.
                              */

/** @def err
 * Prints an error to stderr
 * @param err_string error message
 */
#define err(err_string) { (void)fprintf(stderr, err_string); }

/** @def die
 * Prints an error to stderr and exits
 * @param err_string error message
 */
#define die(err_string) { (void)fprintf(stderr, err_string); exit(EXIT_FAILURE); }


void bios_init(char *name);
void bios_shutdown();
void bios_read(int number, char *sector);
void bios_write(int number, char *sector);

/* The functions that need to be implemented by the students */

/* initializes the filesystem driver */
void fs_init();

/* input: the path of the file to be opened 
   return: a file descriptor for the opened file */
int fs_open(const char *path);

/* creates an empty file at path 
   returns the file descriptor for the newly created file */
int fs_creat(const char *path);

/* input/output: as the linux read() function */
int fs_read(int fd, void *buffer, int len);

/* input/output: as the linux write() function */
int fs_write(int fd, void *buffer, int len);

/* closes the file with the specified descriptor*/
void fs_close(int fd);
