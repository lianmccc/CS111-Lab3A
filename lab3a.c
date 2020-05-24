#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include "ext2_fs.h"

int device_fd;
struct ext2_super_block superblock;
struct ext2_group_desc groupdesc;
long block_bitmap_offset;
long inode_bitmap_offset;
char *block_bitmap;
char *inode_bitmap;

int block_size;

// takes a block number and return its offset 
long block_offset(int block_num) {
	return 1024 + (block_num - 1) * block_size;
}

// read super block 
void read_superblock() {
    pread(device_fd, &superblock, sizeof(superblock), 1024);

    // verify if it is superblock
    if (superblock.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "is not superblock\n");
		exit(2);
    }

    // get the block size from superblock
    block_size = EXT2_MIN_BLOCK_SIZE << superblock.s_log_block_size;
}

// print super block summary 
void print_superblock() {
    // superblock summary
	fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", 
        superblock.s_blocks_count,      // total number of blocks (decimal)
        superblock.s_inodes_count,      // total number of i-nodes (decimal)
        block_size,	                    // block size (in bytes, decimal)
        superblock.s_inode_size,        // i-node size (in bytes, decimal)
        superblock.s_blocks_per_group,  // blocks per group (decimal)
        superblock.s_inodes_per_group,  // i-nodes per group (decimal)
        superblock.s_first_ino          // first non-reserved i-node (decimal)
	);
}

// read block group descriptor table 
void read_groupdesc() {
    // block size must be read before calling this function
    pread(device_fd, &groupdesc, sizeof(groupdesc), block_size + 1024);
}

// print group summary 
void print_groupdesc() {
    int num_blocks_in_this_group = (superblock.s_blocks_per_group < superblock.s_blocks_count) ? superblock.s_blocks_per_group : superblock.s_blocks_count;
    int num_inodes_in_this_group = (superblock.s_inodes_per_group < superblock.s_inodes_count) ? superblock.s_inodes_per_group : superblock.s_inodes_count;

    fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
        superblock.s_block_group_nr,    // group number (decimal, starting from zero
        num_blocks_in_this_group,       // total number of blocks in this group (decimal)
        num_inodes_in_this_group,       // total number of i-nodes in this group (decimal)
        groupdesc.bg_free_blocks_count, // number of free blocks (decimal)
        groupdesc.bg_free_inodes_count, // number of free i-nodes (decimal)
        groupdesc.bg_block_bitmap,      // block number of free block bitmap for this group (decimal)
        groupdesc.bg_inode_bitmap,      // block number of free i-node bitmap for this group (decimal)
        groupdesc.bg_inode_table        // block number of first block of i-nodes in this group (decimal)
    );
}

// free the allocated memory for block bitmap
void free_block_bitmap() {
    free(block_bitmap);
}

// read in block bitmap
void read_block_bitmap() {
    // get block_bitmap offset
    block_bitmap_offset = block_offset(groupdesc.bg_block_bitmap);

    // allocate memory for block_bitmap
    block_bitmap = (char *) malloc(block_size);

    // read block_bitmap 
    pread(device_fd, block_bitmap, block_size, block_bitmap_offset);

    atexit(free_block_bitmap);
}

// check if kth bit of bitmap A is used (1=used, 0=free)
int used(char *A, int k) {
    return A[(k/8)] & (1 << (k % 8));
}

// scan the block bitmap. print each free block
void print_free_blocks() {
    unsigned int i; 

    // print free blocks
    for (i = 0; i < superblock.s_blocks_per_group; i++) {
        int block_num = i + 1;

        if (!used(block_bitmap, i)) 
            fprintf(stdout, "BFREE,%d\n", block_num);
    }
}

// free the allocated memory for inode bitmap
void free_inode_bitmap() {
    free(inode_bitmap);
}

// read in inode bitmap 
void read_inode_bitmap() {
    // get inode bitmap offset
    inode_bitmap_offset = block_offset(groupdesc.bg_inode_bitmap);

    // allocated memory for inode_bitmap
    inode_bitmap = (char *) malloc(block_size);

    // read inode_bitmap
    pread(device_fd, inode_bitmap, block_size, inode_bitmap_offset);

    atexit(free_inode_bitmap);
}

// scan the inode bitmap. print each free inode.
void print_free_inodes() {
    unsigned int i;

    // print free inodes 
    for (i = 0; i < superblock.s_inodes_per_group; i++) {
        int inode_num = i + 1;

        if (!used(inode_bitmap, i)) 
            fprintf(stdout, "IFREE,%d\n", inode_num);
    }
}

// give block number and parent_inode, print the directory entries summery
void read_dir_ent(unsigned int parent_inode, unsigned int block_num) {
	struct ext2_dir_entry dir_ent;
	long datablk_offset = block_offset(block_num);
	unsigned int num_bytes = 0;


}

// given inode number, print inode summery
void print_inode(int inode_num) {
    struct ext2_inode inode;

	long inode_offset = block_offset(groupdesc.bg_inode_table) + (inode_num - 1) * sizeof(inode);
	pread(device_fd, &inode, sizeof(inode), inode_offset);
  
    char filetype;


    int file_format = (inode.i_mode >> 12) << 12;
    // ('f' for file, 'd' for directory, 's' for symbolic link, '?" for anything else)
	if (file_format == S_IFREG) {
		filetype = 'f';
	} else if (file_format == S_IFLNK) {
		filetype = 's';
	} else if (file_format == S_IFDIR) {
		filetype = 'd';
	} else if (file_format == 0) {
        return;
    } else {
        filetype = '?';
    }

    fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,",
        inode_num,              // inode number (decimal)
        filetype,               // file type 
        inode.i_mode & 0xFFF,   // mode (low order 12-bits, octal ... suggested format "%o")
        inode.i_uid,            // owner (decimal)
        inode.i_gid,            // group (decimal)
        inode.i_links_count     // link count (decimal)
    );

    // time of last I-node change (mm/dd/yy hh:mm:ss, GMT)
    time_t rawtime = inode.i_ctime;
    struct tm *ptm = gmtime(&rawtime);
    fprintf(stdout, "%02d/%02d/%02d %02d:%02d:%02d,", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_year%100, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

    // modification time (mm/dd/yy hh:mm:ss, GMT)
    rawtime = inode.i_mtime;
    ptm = gmtime(&rawtime);
    fprintf(stdout, "%02d/%02d/%02d %02d:%02d:%02d,", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_year%100, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

    // time of last access (mm/dd/yy hh:mm:ss, GMT)
    rawtime = inode.i_atime;
    ptm = gmtime(&rawtime);
    fprintf(stdout, "%02d/%02d/%02d %02d:%02d:%02d,", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_year%100, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

    fprintf(stdout, "%d,%d", 
	    inode.i_size,           // file size (decimal)
		inode.i_blocks          // number of (512 byte) blocks of disk space (decimal) taken up by this file
	);

    // For ordiary files (type 'f') and directories (type 'd') 
    // the next 15 fields are block addresses (decimal) 
    unsigned int i;
	for (i = 0; i < 15; i++) {
        if (filetype == 'f' || filetype == 'd'){
            fprintf(stdout, ",%d", inode.i_block[i]);
        } else if (filetype == 's' && inode.i_size > 60){
            fprintf(stdout, ",%d", inode.i_block[i]);
        }
	}
	fprintf(stdout, "\n");

    // first 12 are direct blocks
	for (i = 0; i < 12; i++) {
		if (inode.i_block[i] != 0 && filetype == 'd') {
			read_dir_ent(inode_num, inode.i_block[i]);
		}
	}

    //one indirect
    if (inode.i_block[12] != 0) {

    }

    //one double indirect
    if (inode.i_block[13] != 0) {

    }

    //one triple indirect
    if (inode.i_block[14] != 0) {

    }
}

// scan the inode bitmap. print each used inode
void print_inodes() {
    unsigned int i;

    // print free inodes 
    for (i = 0; i < superblock.s_inodes_per_group; i++) {
        int inode_num = i + 1;
        if (used(inode_bitmap, i))
            print_inode(inode_num);
    }
}

int main (int argc, char* argv[]) {
	if (argc != 2) {
		fprintf(stderr, "bad arguments\n");
		exit(1);
	}

    device_fd = open(argv[1], O_RDONLY);

    read_superblock();
    print_superblock();

    read_groupdesc();
    print_groupdesc();

    read_block_bitmap();
    print_free_blocks();
    
    read_inode_bitmap();
    print_free_inodes();

    print_inodes();

    exit(0);
}