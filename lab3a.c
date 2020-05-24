#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
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
    block_bitmap = (char *) malloc(sizeof(block_size));

    // read block_bitmap 
    pread(device_fd, block_bitmap, sizeof(block_size), block_bitmap_offset);

    atexit(free_block_bitmap);
}

// check if kth bit of bitmap A is used (1=used, 0=free)
int used(char *A, int k) {
    return A[(k/8)] & (1 << (k % 8));
}

// scan the block bitmap. print each free block
void print_free_blocks() {
    int i; 

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
    inode_bitmap = (char *) malloc(sizeof(block_size));

    // read inode_bitmap
    pread(device_fd, inode_bitmap, sizeof(block_size), inode_bitmap_offset);

    atexit(free_inode_bitmap);
}

// scan the inode bitmap. print each free inode.
void print_free_inodes() {
    int i;

    // print free inodes 
    for (i = 0; i < superblock.s_inodes_per_group; i++) {
        int inode_num = i + 1;

        if (!used(inode_bitmap, i)) 
            fprintf(stdout, "IFREE,%d\n", inode_num);
    }
}

void print_inode(int inode_num) {
    // TODO: print the inode summary for each inode, given inode_num
    // fprintf(stdout, "INODE,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    
    // );
}

// scan the inode bitmap. print each inode
void print_inodes() {
    int i;

    // print free inodes 
    for (i = 0; i < superblock.s_inodes_per_group; i++) {
        int inode_num = i + 1;
        if (used(inode_bitmap, i))
            print_inode(inode_num);
    }
}

void main (int argc, char* argv[]) {
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


    exit(0);
}