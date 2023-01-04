#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

// for the 8M file system
#define TFS_MAGIC  0x345f2022

#define NUM_BLOCKS 2048
#define NUM_INODES 512 
#define NUM_DENTRIES_PER_BLOCK 128

#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

#define BITS_PER_UINT 32

// file type
#define REGULAR 1
#define DIR 2

struct tfs_superblock {
	int signature ; // Signature 
	int num_blocks; // number of blocks; same as NUM_BLOCKS in this project
	int num_inodes; // number of inodes; same as NUM_INODES in this project
	int root_inode; // inode number of root directory ; use 1
	unsigned int block_in_use[NUM_BLOCKS/BITS_PER_UINT]; //Bitmap for blocks
	unsigned int inode_in_use[NUM_INODES/BITS_PER_UINT]; //Bitmap for inode tables
};

struct tfs_dir_entry { //Entry of root directory. Filename and inode stored here.
	int valid; 
	char fname[24];
    int inum;
};

struct tfs_inode { //Inode entry in Inode table block array
	int type;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union tfs_block {
	struct tfs_superblock super; //Superblock
	struct tfs_inode inode[INODES_PER_BLOCK]; //Inode table block array of inode entries
	struct tfs_dir_entry dentry[NUM_DENTRIES_PER_BLOCK]; //Root directory's directory entries
	int pointers[POINTERS_PER_BLOCK]; //Indirect data block
	char data[DISK_BLOCK_SIZE]; //Raw data of a single data block
};

void tfs_debug() { 
    int i;
    int b_in_use = 0;
	union tfs_block block;

	disk_read(0,block.data);

	int j, k, n; //Indexes for the for-loops.
	int ino_in_use = 0; //Stores the amount of inodes used.
	struct tfs_inode rNode;
	struct tfs_superblock sBlock = block.super; //Store superblock to sBlock to reference.
	union tfs_block rootDir; //Store root directory to reference.

	printf(" superblock:\n");

    // check signature
	if(block.super.signature  == TFS_MAGIC) {
        printf("      signature is valid\n");
	}
    else {
        printf("      signature is invalid\n");
	}

    for(i = 0; i < NUM_BLOCKS; i++) {
		if(block.super.block_in_use[i/BITS_PER_UINT] & (1 << (i % BITS_PER_UINT))) {
            b_in_use++ ;
		}
	}
	printf("      %d blocks in use \n", b_in_use); 

	//Count inodes in use
	for (i = 1; i <= NUM_INODES/INODES_PER_BLOCK; i++) { //For-loop that reads all inode table blocks.
		//disk_read(i, block.data); //FOR DEBUGGING, DELETE LATER

		for (j = 0 + INODES_PER_BLOCK * (i - 1); j < INODES_PER_BLOCK + INODES_PER_BLOCK * (i - 1); j++) { //For-loop that reads through the maximum amount of inodes per block. Uses "i" for number of iterations.
			if (sBlock.inode_in_use[j/BITS_PER_UINT] & (1 << (j % BITS_PER_UINT))) { //Uses the bitmap to see if the corresponding inode is set to 1, which means used.
				ino_in_use++; //Increase count of used inodes if inode was set to 1 in bitmap. If not, keep looping till limit is reached.
			}
		}
	}
	printf("      %d inodes in use\n", ino_in_use);

	//Get root inode information
	disk_read(sBlock.root_inode / INODES_PER_BLOCK + 1, block.data); //Read the block that has the root inode.
	rNode = block.inode[sBlock.root_inode]; //Store root inode to rNode so it can be referenced while switching to other blocks.

	printf(" root inode %d:\n", sBlock.root_inode);
	printf("      size: %d bytes\n", rNode.size);
	printf("      direct blocks:");
	for (i = 0; i < POINTERS_PER_INODE && rNode.direct[i] != 0; i++) { //Loop used to retrieve direct blocks. 
		printf(" %d", rNode.direct[i]);
	}
	printf("\n");

	//Explore root directory
	for (i = 0; i < POINTERS_PER_INODE && rNode.direct[i] != 0; i++) { //Loop that reads through each available root directory in case there is more than 1.
		disk_read(rNode.direct[i], block.data);
		rootDir = block;

		for (j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) { //Loop used to check all directory entries within a root directory block.
			if (rootDir.dentry[j].valid != 0) { //Check if directory is valid, if not then skip.
				printf(" %s inode %d:\n", rootDir.dentry[j].fname, rootDir.dentry[j].inum);
				n = rootDir.dentry[j].inum % INODES_PER_BLOCK; //Retrieve inode number relative within a block (0 to 127, not global 0 to 511).
				disk_read(rootDir.dentry[j].inum / INODES_PER_BLOCK + 1, block.data); //Find what block contains the inode as to get file's size, direct and indirect data blocks.
				printf("      size: %d bytes\n", block.inode[n].size);

				printf("      direct blocks:");
				for (k = 0; k < POINTERS_PER_INODE && block.inode[n].direct[k] != 0; k++) { //Loop that iterates through each direct data block pointed by the inode.
					printf(" %d", block.inode[n].direct[k]);
				}
				printf("\n");

				if (block.inode[n].indirect != 0) { //Check if indirect block is used, if it is then check all indirect data blocks.
					printf("      indirect block: %d\n", block.inode[n].indirect);

					disk_read(block.inode[n].indirect, block.data); //Read the indirect block that points to indirect data blocks.
					printf("      indirect data blocks:");
					for (k = 0; k < POINTERS_PER_BLOCK && block.pointers[k] != 0; k++) { //Loop that iterates through each indirect data block
						printf(" %d", block.pointers[k]);
					}
					printf("\n");
				}

				//disk_read(rNode.direct[i], block.data); //Since disk_read() was used for indirect, must set it back to root directory block for rest of loop.
			} //NOTE: Better implementation could be made to lessen the amount of disk reads by making temporary blocks that reference root directory. Same outcome would result either way.
		}
	}
}

int tfs_delete(const char *filename) { //Deletes file by unlinking direct and indirect datablocks, the indirect block, the directory entry, and the inode of the file given.
	int blocksDeleted = 0, i, j, k, n; //n used for storing global inode number (0 to 511).
	struct tfs_inode tempNode;
	struct tfs_inode rNode;
	struct tfs_superblock sBlock;
	union tfs_block block, tempBlock; //tempBlock used for storing indirect block while reading other blocks.

	//Get root inode from superblock for 1st for-loop and accessing the root directory entry of the given file.
	disk_read(0, block.data);
	sBlock = block.super;
	disk_read(sBlock.root_inode / INODES_PER_BLOCK + 1, block.data);
	rNode = block.inode[sBlock.root_inode];

	for (i = 0; i < POINTERS_PER_INODE && rNode.direct[i] != 0; i++) { //Iterate through multiple root directories if more than 1.
		disk_read(rNode.direct[i], block.data); //Read block of root directory.

		for (j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) { //Loop through root directory enties within root directory.
			if (block.dentry[j].valid != 0 && !strcmp(filename, block.dentry[j].fname)) { //If directory entry is valid and given filename matches with the entry's filename, then file was found.
				n = block.dentry[j].inum; //Store global inode number corresponding to given file into n.
				disk_read(n / INODES_PER_BLOCK + 1, block.data); //Read block that contains relevant inode.
				tempNode = block.inode[n % INODES_PER_BLOCK]; //Store relevant inode entry into tempNode.

				disk_read(0, block.data); //Read superblock to update block bitmap and write changes back to disk image.
				for (k = 0; k < POINTERS_PER_INODE && tempNode.direct[k] != 0; k++) { //Loop through each direct data block pointed by inode.
					block.super.block_in_use[tempNode.direct[k] / BITS_PER_UINT] &= ~(1 << (tempNode.direct[k] % BITS_PER_UINT)); //Set block pointed by inode to 0 in block bitmap to indicate it is no longer used.

					blocksDeleted++; //Increase the count of blocks deleted.
				}
				disk_write(0, block.data); //Write changes done back to disk image.

				if (tempNode.indirect != 0) { //If inode points to an indrect block, then enter to update block bitmap further.
					disk_read(tempNode.indirect, block.data); //Read indirect block pointed by inode of given file.
					tempBlock = block; //Store indirect block to access its pointers while reading other blocks.
					disk_read(0, block.data); //Read superblock to update block bitmap and write changes back to disk image.

					for (k = 0; k < POINTERS_PER_BLOCK && tempBlock.pointers[k] != 0; k++) { //Loop through each indirect data block pointed by indirect block pointers.
						block.super.block_in_use[tempBlock.pointers[k] / BITS_PER_UINT] &= ~(1 << (tempBlock.pointers[k] % BITS_PER_UINT)); //Set block pointed by indirect pointer to 0 in block bitmap to indicate it is no longer used.

						blocksDeleted++; //Increase the count of blocks deleted.
					}
					block.super.block_in_use[tempNode.indirect / BITS_PER_UINT] &= ~(1 << (tempNode.indirect % BITS_PER_UINT)); //After loop, set indirect block itselft to 0 in block bitmap.
					disk_write(0, block.data); //Write changes done back to disk image.
				}

				if (blocksDeleted != 0) { //If at least one block was deleted, then deletion should have been successful.
					//free inode
					block.super.inode_in_use[n / BITS_PER_UINT] &= ~(1 << (n % BITS_PER_UINT)); //Use inode bitmap to set inode related to file to 0 as it is no longer used. Current block is superblock so no read is needed.
					disk_write(0, block.data); //Write change back to disk.
					disk_read(rNode.direct[i], block.data); //Read root directory block.
					block.dentry[j].valid = 0; //Set file entry in root directory to be invalid as it is no longer used. If not done, deleted blocks and inode will still "exist."
					disk_write(rNode.direct[i], block.data); //Write change back to disk.

					return n; //Return global inode number that was deleted.
				}
				else { //If no blocks were deleted somehow, return 0.
					return 0;
				}
			}
		}
	}

	return 0; //If file could not be found, return 0.
}

int tfs_get_inumber(const char *filename) { //Retrieves inode number corresponding to given filename.
	int i, j;
	struct tfs_inode rNode;
	struct tfs_superblock sBlock;
	union tfs_block block;

	//Get root inode from superblock for for-loop
	disk_read(0, block.data);
	sBlock = block.super;
	disk_read(sBlock.root_inode / INODES_PER_BLOCK + 1, block.data);
	rNode = block.inode[sBlock.root_inode];

	for (i = 0; i < POINTERS_PER_INODE && rNode.direct[i] != 0; i++) { //Iterate through root directories pointed by root inode.
		disk_read(rNode.direct[i], block.data);

		for (j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) { //Loop through valid directory entries to find file given.
			if (block.dentry[j].valid != 0 && !strcmp(filename, block.dentry[j].fname)) { //If valid, check if the filenames match. If so, file was found in the root directory.
				return block.dentry[j].inum; //Return inode number stored within the file's corresponding root directory entry.
			}
		}
	}

	return 0; //If file could not be found, return 0.
}

int tfs_getsize(const char *filename) { //Returns size attribute from the inode of the given file.
	int i, j, n; //n used to store inode number relevant within its block (inode table).
	struct tfs_inode rNode;
	struct tfs_superblock sBlock;
	union tfs_block block;

	//Get root inode from superblock for for-loop
	disk_read(0, block.data);
	sBlock = block.super;
	disk_read(sBlock.root_inode / INODES_PER_BLOCK + 1, block.data);
	rNode = block.inode[sBlock.root_inode];

	for (i = 0; i < POINTERS_PER_INODE && rNode.direct[i] != 0; i++) { //Iterate through root directories pointed by root inode.
		disk_read(rNode.direct[i], block.data);

		for (j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) { //Loop through valid directory entries to find file given.
			if (block.dentry[j].valid != 0 && !strcmp(filename, block.dentry[j].fname)) { //If valid, check if the filenames match. If so, file was found in the root directory.
				n = block.dentry[j].inum % INODES_PER_BLOCK; //Get inode number relevant within the block the inode is stored in.
				disk_read(block.dentry[j].inum / INODES_PER_BLOCK + 1, block.data); //Read block that contains inode related to file given.
				return block.inode[n].size; //Return file size stored in the inode entry.
			}
		}
	}

	return -1; //If file could not be found, return -1.
}

int tfs_read(int inumber, char *data, int length, int offset) { //Scans through data blocks of provided inode. Bytes scanned limited to size of buffer.
	int bytesRead = 0, i = offset / DISK_BLOCK_SIZE, j; //i is set here in case there is a switch from direct data blocks to indirect data blocks. i will correspond to pointer (direct or indirect) the offset last left off from.
	struct tfs_inode tempNode;
	union tfs_block block, tempBlock; //tempBlock used to store indirect block of pointers.

	disk_read(inumber / INODES_PER_BLOCK + 1, block.data); //Read block that contains the inode given.
	tempNode = block.inode[inumber % INODES_PER_BLOCK]; //Store this inode entry for later use while reading through other blocks.

	strcpy(data, "\0"); //Reset buffer as it wasn't done in shell.c. If not done, will cause overflow of buffer.

	//Exhaust direct data blocks
	while (i < POINTERS_PER_INODE) { //Loop through all direct data blocks.
		disk_read(tempNode.direct[i], block.data); //Read block pointed by given inode.

		for (j = 0; j < DISK_BLOCK_SIZE; j++) { //Read through raw data of data block byte-by-byte (j) until block size is reached. 
			if (bytesRead + offset == tempNode.size) { //If the amount of bytes locally read plus the amount of bytes globally read before this call are equal to the size
				return bytesRead;					   //of the file corresponding to the given inode, then return the amount of bytes read locally within this call of tfs_read().
			}
			if (bytesRead == length) { //If the amount of bytes locally read within this call match the size of the buffer, then return the amount of bytes read as buffer is filled.
				return bytesRead;
			}
			
			//data += block.data[j]; //Not working, not sure why.
			strncat(data, &block.data[j], 1); //Copy selected byte of data from data block and paste it to buffer.
			bytesRead++; //Increase the amount of bytes read because of above action.
		}

		i++; //Iterate to next direct data block.
	}

	//Exhaust indirect data blocks, if any
	if (tempNode.indirect == 0) { //If no indirect block is found, then return amount of bytes locally read.
		return bytesRead;
	}

	disk_read(tempNode.indirect, block.data); //Read block containing indirect data block pointers.
	tempBlock = block; //Store indirect block to access pointers while reading through the blocks pointed by the indirect block.
	while (i - POINTERS_PER_INODE < POINTERS_PER_BLOCK) { //Loop through the indirect data blocks. Use i - POINTERS_PER_INODE since it will not start at 0 when code reaches here.
		disk_read(tempBlock.pointers[i - POINTERS_PER_INODE], block.data); //Read block pointed by indirect block.

		for (j = 0; j < DISK_BLOCK_SIZE; j++) { //Read through raw data of indirect data block byte-by-byte (j) until block size is reached.
			if (bytesRead + offset == tempNode.size) { //If the amount of bytes locally read plus the amount of bytes globally read before this call are equal to the size
				return bytesRead;					   //of the file corresponding to the given inode, then return the amount of bytes read locally within this call of tfs_read().
			}
			if (bytesRead == length) { //If the amount of bytes locally read within this call match the size of the buffer, then return the amount of bytes read as buffer is filled.
				return bytesRead;
			}

			//data += block.data[j]; //Not working either, not sure why.
			strncat(data, &block.data[j], 1); //Copy selected byte of data from data block and paste it to buffer.
			bytesRead++; //Increase the amount of bytes read because of above action.
		}

		i++; //Iterate to next indirect data block.
	}

	return bytesRead; //If both while-loops and the if-statement fail, return bytesRead which will be 0.
}