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
char *block_bitmap;
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
    fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
        superblock.s_block_group_nr,    // group number (decimal, starting from zero
        superblock.s_blocks_per_group,  // total number of blocks in this group (decimal)
        superblock.s_inodes_per_group,  // total number of i-nodes in this group (decimal)
        groupdesc.bg_free_blocks_count, // number of free blocks (decimal)
        groupdesc.bg_free_inodes_count, // number of free i-nodes (decimal)
        groupdesc.bg_block_bitmap,      // block number of free block bitmap for this group (decimal)
        groupdesc.bg_inode_bitmap,      // block number of free i-node bitmap for this group (decimal)
        groupdesc.bg_inode_table        // block number of first block of i-nodes in this group (decimal)
    );
}

// thest the bit at the kth bit in the bit array A;
int check_bit(int *A, int k) {
    return A[(k/8)] & (1 << (k % 8)); 
}

// scan the free block bitmap for each group (there is only one group for this lab)
void print_free_blocks() {
    int i; 
    long bitmap_offset = block_offset(groupdesc.bg_block_bitmap);
    block_bitmap = (char *) malloc(sizeof(block_size));
    pread(device_fd, block_bitmap, sizeof(block_size), bitmap_offset);
    for (i = 0; i < superblock.s_blocks_per_group; i++) {
        if (checkBit(block_bitmap, i)) {
            fprintf("BFREE,%d\n", i + 1);
        }
    }
    free(block_bitmap);
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

    print_free_blocks();
    

    exit(0);
}