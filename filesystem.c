#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h> // For boolean data type
#include<string.h> // For manipulating strings
#include<unistd.h> // low level file and directory handling/operations
#include<fcntl.h> // file control options

/*
 *   ___ ___ ___ ___ ___ ___ ___ ___ ___ ___ ___ 
 *  |   |   |   |   |                       |   |
 *  | 0 | 1 | 2 | 3 |     .....             |127|
 *  |___|___|___|___|_______________________|___|
 *  |   \    <-----  data blocks ------>
 *  |     \
 *  |       \
 *  |         \
 *  |           \
 *  |             \
 *  |               \
 *  |                 \
 *  |                   \
 *  |                     \
 *  |                       \
 *  |                         \
 *  |                           \
 *  |                             \
 *  |                               \
 *  |                                 \
 *  |                                   \
 *  |                                     \
 *  |                                       \
 *  |                                         \
 *  |                                           \
 *  |     <--- super block --->                   \
 *  |______________________________________________|
 *  |               |      |      |        |       |
 *  |        free   |      |      |        |       |
 *  |       block   |inode0|inode1|   .... |inode15|
 *  |        list   |      |      |        |       |
 *  |_______________|______|______|________|_______|
 */

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 128
#define NUM_INODES 16
#define FILENAME_MAXLEN 8
int myfs;

// ------------------------------ Defining Structs for Inode and Dirent ------------------------------ //

/* inode */
typedef struct inode {
    int dir;                    // 1 if it's a directory, 0 if it's a file
    char name[FILENAME_MAXLEN]; // Name of the file or directory
    int size;                   // Size of the file or directory in bytes
    int blockptrs[8];           // Direct pointers to data blocks
    int used;                   // 1 if the entry is in use
    int rsvd;                   // Reserved for future use
} inode;

/* directory entry */
typedef struct dirent {
    char name[FILENAME_MAXLEN]; // Name of the entry
    int namelen;                // Length of entry name
    int inode;                  // Index of the corresponding inode
} dirent;

// ------------------------------ Initializing File System - MYFS ------------------------------ //

int init(){
    int myfs = open("./myfs", O_CREAT | O_RDWR, 0666); // create a file named myfs with read write enabled
    if(myfs == -1){
        printf("Error: Cannot create file system myfs\n"); return -1;
    }

    ftruncate(myfs, BLOCK_SIZE * NUM_BLOCKS); // 128 * 1024 = 128KB allocated to myfs

    char fs = 'A'; write(myfs, (char*)&fs, 1);
    char dbm = (char)1; write(myfs, (char*)&dbm, 1);
    // writes 'A' and 1 to the first two bytes of myfs - identification and directory bitmap, 'A' for ident, 1 shows it is a directory

    // Initializing the Root Inode
    struct inode root_inode;
    root_inode.dir = 1; // root inode is a directory
    strcpy(root_inode.name, "/"); // first root directory named "/"
    root_inode.size = sizeof(struct dirent);
    root_inode.blockptrs[0] = 1;
    root_inode.used = 1; // yes it is in use
    root_inode.rsvd = 0; // no it is not reserved for future use
    
    // write the root inode into myfs
    lseek(myfs, NUM_BLOCKS, SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));

    // Initializing the Root Directory Entry
    struct dirent root_dirent;
    strcpy(root_dirent.name, ".");
    root_dirent.namelen = 1;
    root_dirent.inode = 0;

    // write the root directory entry into myfs
    lseek(myfs, BLOCK_SIZE, SEEK_SET); write(myfs, (char*)&root_dirent, sizeof(struct dirent));

    return myfs;
}

// ------------------------------ Functions Prototyping ------------------------------  //
// 1. create file
// 2. remove/delete file
// 3. copy file
// 4. move a file
// 5. create directory
// 6. remove a directory
// 7. list file info
int CR(char* filename, int size);
int DL(char* filename);
int CP(char* srcname, char* dstname);
int MV(char* srcname, char* dstname);
int CD(char* dirname);
int DD(char* dirname);
void LL();

// ------------------------------ Helpers Along the Way ------------------------------ //
int findAvailableInode(){
    /*Finds and returns the first available inode in myfs. It reads the inode data, iterates over it, and returns the index of the first unused inode. If no available inodes are found, it shown an error message and returns -1*/
    struct inode inodes[NUM_INODES];
    lseek(myfs, NUM_BLOCKS, SEEK_SET); read(myfs, (char*)inodes, NUM_INODES * sizeof(struct inode));
    for(int i = 0; i < NUM_INODES; i++){
        if(inodes[i].used == 0) return i;
    }
    printf("Error: No available inodes\n");
    return -1;
}

int findParentInode(char* filename){
    /*Finds the inode of the parent directory for a given file/dir path by splitting the path based on '/' token, iterating over entries in each directory, and checking if the directory exists in the path or not. If directory is found, its inode is returned */
    if(strcmp(filename, "/") == 0){ // '/' is the root directory, hence error and returns an error
        printf("Error: File name cannot be the root directory\n"); return -1;
    }

    char directory[100]; // buffer to store the directory name 
    int directory_inode = 0; // initialize the directory inode to 0, which is the root directory inode
    struct dirent root_dirent; 
    struct inode root_dirinode, root_inode; 

    // splitting the path based on '/' token and iterating over each directory
    while(sscanf(filename, "/%[^/]%s", directory, filename) == 2){
        bool flag = false; //flag to indicate if directory found

        // read the inode of the parent directory from myfs 
        lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_dirinode, sizeof(struct inode));
        int size = root_dirinode.size;

        for(int i = 0; i < size; i += sizeof(struct dirent)){ //iterate through the entries in the parent directory
            lseek(myfs, BLOCK_SIZE * root_dirinode.blockptrs[0] + i, SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));
            if(strcmp(root_dirent.name, directory) == 0){ //if current entry name matches the directory name, read the inode of the block, check if it is a directory, set flag to true and break
                lseek(myfs, NUM_BLOCKS + root_dirent.inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
                if(root_inode.dir == 1){
                    directory_inode = root_dirent.inode; flag = true; break;
                } 
            }
        }
        if(flag == false){ // indicates directory was not found in the path, hence error
            printf("Error: Directory '%s' in the provided path doesn't exist\n", directory); return -1;
        }
    }
    sscanf(filename, "/%s", filename); // removes the leading '/' from the filename
    return directory_inode;
}

int findAvailableDataBlock(int* blockpointers, int blockcount){
    /*Finds the available data blocks by reading the block occupancy status, and iterating through the data blocks, assigning the indices to the blockpointers. If no available data blocks then an error is shown */
    int dbi = 1; //Initialize data block index to 1
    char occupado[NUM_BLOCKS]; //array to store block occupancy status
    lseek(myfs, 0, SEEK_SET); read(myfs, occupado, NUM_BLOCKS);

    for(int i = 0; i < blockcount; i++){
        bool flag = false; //flag to indicate if aviailable block found
        while(dbi <= BLOCK_SIZE){
            if(occupado[dbi] != (char)1){ // If block not occupied, assign index of available block to blockpointers, increment index, set flag to true, break inner loop and move onto next index
                blockpointers[i] = dbi; dbi++;
                flag = true; break;
            }
            dbi++;
        }
        if(flag == false){ //if no data blocks found, print error.
            printf("Error: No available data blocks\n"); return -1;
        }
    }
    return 0;
}

int assassin(char* filename, int directory_inode, int node, int dir){
    // searches for the given path/filename/dirname then writes it into a directory if not found - hence the analogy of assassin xD
    struct inode root_inode, t_inode;
    struct dirent curr_entry;
    
    // Read the inode of the parent directory from myfs
    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
    
    for(int i = 0; i < root_inode.size; i += sizeof(struct dirent)){ // iterate through the entries in the parent directory
        lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[0] + i, SEEK_SET); read(myfs, (char*)&curr_entry, sizeof(struct dirent));
        if(strcmp(curr_entry.name, filename) == 0){ // if current entry name matches the filename, read the inode of the block, check if it is a directory, set flag to true and break
            lseek(myfs, NUM_BLOCKS + curr_entry.inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&t_inode, sizeof(struct inode));
            if(t_inode.dir == dir){ // if the entry is of the same type as the one we are trying to create, print error and return
                if(dir == 0) printf("Error: The file '%s' already exists\n", filename);
                else printf("Error: The directory '%s' already exists\n", filename);
                return -1;
            }
        }
    }
    // if the entry is not found, write it into the parent directory
    lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[0] + root_inode.size, SEEK_SET);
    curr_entry.namelen = strlen(filename); strcpy(curr_entry.name, filename);
    curr_entry.inode = node;
    write(myfs, (char*)&curr_entry, sizeof(struct dirent));

    // update the parent directory's size, and write the updated inode of the parent directory back into myfs)
    root_inode.size += sizeof(struct dirent);
    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));
    return 0;
}

int stalker(char* filename, int* block, int* finode, int directory_inode, int dir){
    /*Searches for a file or directory specified by its inode - directory_inode (hence the name stalker xD). It returns the block index of the found entry (if found), and also updates 'finode' with the corresponding inode number.*/
    struct inode root_inode; struct dirent root_dirent;
    // Read the inode of the parent directory from myfs
    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
    int size = root_inode.size, val = -1, i = 0; *block = root_inode.blockptrs[0]; // initialize the block index to the first block of the parent directory
    while(i < size){ // iterate through the entries in the parent directory
        lseek(myfs, BLOCK_SIZE * (*block) + i, SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));
        if(strcmp(root_dirent.name, filename) == 0){ // if current entry name matches the filename, update val and finode, and break. val -2 represents entry found but not the required type
            val = -2; *finode = root_dirent.inode;
            struct inode root_inode;
            lseek(myfs, NUM_BLOCKS + root_dirent.inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
            if(root_inode.dir == dir) return i; // if the entry is of the same type as the one we are trying to find, return the block index
        }
        i += sizeof(struct dirent);
    }
    return val;
}

int execution(int directory_inode, int directory_entry){
    /*Removes / deletes an entry from a directory specified by its inode at the given entry offset. Hence the name executioner xD - since it 'executes'(deletes) an entry */
    struct inode root_inode; struct dirent root_dirent;

    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
    int size = root_inode.size, blockOff = root_inode.blockptrs[0];

    if(directory_entry != size - sizeof(struct dirent)){ // if the entry to be deleted is not the last entry in the directory, replace it with the last entry
        lseek(myfs, BLOCK_SIZE * blockOff + size - sizeof(struct dirent), SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));
        lseek(myfs, BLOCK_SIZE * blockOff + directory_entry, SEEK_SET); write(myfs, (char*)&root_dirent, sizeof(struct dirent));
    }
    // update the size of the directory, and write the updated inode of the directory back into myfs
    root_inode.size = size - sizeof(struct dirent);
    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));

    return 0;
}

void successiveExecution(int finode){
    /*Recursively removes / deletes a file or directory specified by the inode - since its recursive deletion, hence analogy to successive executions - killing spree lessgooo*/
    struct inode root_inode;
    // Read the inode of the file/directory from myfs
	lseek(myfs, NUM_BLOCKS + finode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(inode));

	root_inode.used = 0; // mark inode as unused, and write the updated inode back into myfs
	lseek(myfs, NUM_BLOCKS + finode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));
	
	char nc = '\0', blockData[BLOCK_SIZE]; // null character and block data initialized, with each byte of block data set to null character
	for (int i = 0; i < BLOCK_SIZE; i++) blockData[i] = nc;

	if(root_inode.dir == 1){ // if the inode is of a directory, recursively delete all the files and directories in it
		lseek(myfs, root_inode.blockptrs[0], SEEK_SET); write(myfs, &nc, 1);
		struct dirent root_dirent;
		for(int i = 0; i < root_inode.size; i += sizeof(struct dirent)){
			lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[0], SEEK_SET); read(myfs, &root_dirent, sizeof(struct dirent));
			successiveExecution(root_dirent.inode);
		}
		lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[0], SEEK_SET); write(myfs, &blockData, BLOCK_SIZE);
	}
	else if(root_inode.dir == 0){ // if the inode is of a file, mark the data blocks as unused, and write the null character into the data blocks, thus removing data blocks
        int size = root_inode.size, blockcount = root_inode.size / BLOCK_SIZE;
		if (root_inode.size > blockcount * BLOCK_SIZE) blockcount++;

		for(int i = 0; i < blockcount; i++){
			lseek(myfs, root_inode.blockptrs[i], SEEK_SET); write(myfs, &nc, 1);
			lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[i], SEEK_SET);
			if (size > BLOCK_SIZE){
                write(myfs, blockData, BLOCK_SIZE); size -= BLOCK_SIZE;
            }
			else write(myfs, blockData, size);
		}
		
    }
}

// ------------------------------ Create File ------------------------------ //

int CR(char* filename, int size){ // Create a file with the given filename and size
    struct inode finode;
    
    int directory_inode = findParentInode(filename); // find the inode of the parent directory where the file will be created
    if(directory_inode == -1) return -1;

    int blockcount = (size % BLOCK_SIZE != 0) + (size / BLOCK_SIZE); // number of blocks needed to store the file
    if(blockcount > 8){ // if the file size exceeds the maximum size limit, return an error
        printf("Filesize exceeding size limit\n"); return -1;
    }
    
    if(findAvailableDataBlock(finode.blockptrs, blockcount) == -1) return -1; // find available data blocks

    int available_inode = findAvailableInode(); // find available inode
    if(available_inode == -1) return -1;

    if(assassin(filename, directory_inode, available_inode, 0) == -1) return -1; // write the file into the parent directory

    // Initialize the file inode - finode
    finode.dir = 0; // 0 since its a file, not a directory
    strcpy(finode.name, filename); // set the name of the file
    finode.size = size; // set its size
    finode.used = 1; // yes it is in use
    finode.rsvd = 0; // no it is not reserved for future use

    // write the inode of the file into myfs
    lseek(myfs, NUM_BLOCKS + available_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&finode, sizeof(struct inode));

    char c = (char)1, data[BLOCK_SIZE]; // mark the data block as occupied, and initialize the data array
    int buffsize = BLOCK_SIZE; // buffer size is set to BLOCK_SIZE
    for(int i = 0; i < blockcount; i++){ // iterate through the data blocks
        // mark the data block as occupied
        lseek(myfs, finode.blockptrs[i], SEEK_SET); write(myfs, &c, 1);

        if(size > BLOCK_SIZE) size -= BLOCK_SIZE;
        else buffsize = size;

        // generate random data for the file and write the data into the data block
        for(int j = 0; j < buffsize; j++) data[j] = (char)(97 + (rand() % 26)); 
        lseek(myfs, BLOCK_SIZE * finode.blockptrs[i], SEEK_SET); write(myfs, data, buffsize);
    }
    printf("File '%s' created successfully\n", filename);
    return 0;
}

// ------------------------------ Delete File ------------------------------ //

int DL(char* filename){ // Delete a file with the given filename
    int directory_inode = findParentInode(filename); // find the inode of the parent directory where the file will be deleted
    if(directory_inode == -1) return -1;

    int block, finode; // block and inode of the file to be deleted
    int t_inode = stalker(filename, &block, &finode, directory_inode, 0); // find the inode of the file to be deleted
    if(t_inode < 0){ // if the file is not found, return an error
        printf("Error: File '%s' does not exist\n", filename); return -1;
    }
    // delete the file from the parent directory by removing the directory entry and freeing the inode and data block, then recursively delete the file and its data blocks
    execution(directory_inode, t_inode); successiveExecution(finode);
    printf("File '%s' deleted successfully\n", filename);
    return 0;
}

// ------------------------------ Copy File ------------------------------ //
int CP(char *srcname, char *dstname){ // Copy a file with the given filename to a destination with the given filename
    int src_inode = findParentInode(srcname), dst_inode = findParentInode(dstname); // find the inode of the parent directory where the file will be copied from and copied to
    if(src_inode == -1 || dst_inode == -1) return -1;

    int block_og, block_cp, finode_og, finode_cp; // block and inode of the original file and the file to be copied to
    // find the inode of the file to be copied from and the file to be copied to
    int source_entry = stalker(srcname, &block_og, &finode_og, src_inode, 0), directory_entry = stalker(dstname, &block_cp, &finode_cp, dst_inode, 0);
    if(source_entry < 0){ // if the file to be copied from is not found, return an error
        printf("Error: File '%s' does not exist, or you've provided a directory - can't handle directories\n", srcname); return -1;
    }

    struct inode root_inode;
    lseek(myfs, NUM_BLOCKS + finode_og * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));

    int blockcount = (root_inode.size % BLOCK_SIZE != 0) + (root_inode.size / BLOCK_SIZE); // number of blocks needed to store the file
    int blockData[blockcount]; // array to store the block indices of the file to be copied from
    for(int i = 0; i < blockcount; i++) blockData[i] = root_inode.blockptrs[i];

    strcpy(root_inode.name, dstname); // set the name of the file to be copied to
    
    if(findAvailableDataBlock(root_inode.blockptrs, blockcount) == -1) return -1;
    
    int available_inode = findAvailableInode();
    if(available_inode == -1) return -1;

    if(directory_entry > -1){ // if the file to be copied to already exists, delete it
        struct dirent root_dirent;
        lseek(myfs, BLOCK_SIZE * block_cp + directory_entry, SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));
        successiveExecution(root_dirent.inode); execution(dst_inode, directory_entry);
    }
    // write the updated destination file inode into myfs
    lseek(myfs, NUM_BLOCKS + available_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));

    char temp_data[BLOCK_SIZE]; // temporary data array
    for(int i = 0; i < blockcount; i++){ // copy data from the original file to the copied file
        lseek(myfs, BLOCK_SIZE * blockData[i], SEEK_SET); read(myfs, temp_data, BLOCK_SIZE);
        lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[i], SEEK_SET); write(myfs, temp_data, BLOCK_SIZE);
    }

    struct inode temp_inode; struct dirent temp_dirent;
    // read the inode of the destination directory from myfs
    lseek(myfs, NUM_BLOCKS + dst_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&temp_inode, sizeof(struct inode));

    int size = temp_inode.size, blockOff = temp_inode.blockptrs[0];

    strcpy(temp_dirent.name, dstname); // set the name of the destination file, and update the destination directory by writing the updated directory entry into myfs
    temp_dirent.namelen = strlen(dstname); temp_dirent.inode = available_inode;

    lseek(myfs, BLOCK_SIZE * blockOff + size, SEEK_SET); write(myfs, (char*)&temp_dirent, sizeof(struct dirent));
    // update the size of the destination directory, and write the updated inode of the destination directory back into myfs
    temp_inode.size = size + sizeof(struct dirent);
    lseek(myfs, NUM_BLOCKS + dst_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&temp_inode, sizeof(struct inode));
    printf("File '%s' copied successfully to destination '%s' \n", srcname, dstname);
    return 0;
}


// ------------------------------ Move File ------------------------------ //

int MV(char* srcname, char* dstname){ 
    /* Combines the copy (CP) and delete (DL) operations to move a file from the source location to the destination location. It first copies the source file to the destination and then deletes the source file */
    CP(srcname, dstname); DL(srcname); printf("File '%s' moved successfully to destination '%s' \n", srcname, dstname); return 0;
}

// ------------------------------ Create Directory ------------------------------ //

int CD(char* dirname){ // Create a directory with the given dirname
    int parent_inode = findParentInode(dirname);
    if(parent_inode == -1) return -1; // Return an error if parent directory doesn't exist
    
    int block; // block index of the directory
    if(findAvailableDataBlock(&block, 1) == -1) return -1; // Return an error if no available data blocks

    int available_inode = findAvailableInode();
    if(available_inode == -1) return -1; // Return an error if no available inodes

    if(assassin(dirname, parent_inode, available_inode, 1) == -1) return -1; // Return an error if directory already exists

    struct inode directory_inode; // Initialize the directory inode
    directory_inode.dir = 1; // 1 since its a directory, not a file
    strcpy(directory_inode.name, dirname); // set the name of the directory
    directory_inode.blockptrs[0] = block; // set its block pointer
    directory_inode.size = 0; // 0 since its an empty directry for now
    directory_inode.used = 1; // yes it is in use
    directory_inode.rsvd = 0; // no it is not reserved for future use

    char c = (char)1; 
    // Mark the data block as occupied and write the directory inode into myfs
    lseek(myfs, block, SEEK_SET); write(myfs, &c, 1);
    lseek(myfs, NUM_BLOCKS + available_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&directory_inode, sizeof(struct inode));
    printf("Directory '%s' created successfully\n", dirname);
    return 0;
}

// ------------------------------ Delete Directory ------------------------------ //

int DD(char* dirname){ // Delete a directory with the given dirname
    int directory_inode = findParentInode(dirname); // find the inode of the parent directory where the directory will be deleted
    if(directory_inode == -1) return -1;

    int block, finode; // block and inode of the directory to be deleted
    int t_inode = stalker(dirname, &block, &finode, directory_inode, 1); // find the inode of the directory to be deleted
    if(t_inode < 0){
        printf("Error: Directory '%s' does not exist\n", dirname); return -1;
    }
    // delete the directory from the parent directory by removing the directory entry and freeing the inode and data block, then recursively delete the directory and its data blocks
    execution(directory_inode, t_inode); successiveExecution(finode);
    printf("Directory '%s' deleted successfully\n", dirname);
    return 0;
}

// ------------------------------ List all Files ------------------------------ //

void LL(){ // List all files and directories in the file system
    printf("\n\nMYFS has the following files and directories stored in the system:\n");
    struct inode inodes[NUM_INODES]; // array to store the inodes
    lseek(myfs, NUM_BLOCKS, SEEK_SET); read(myfs, inodes, NUM_INODES * sizeof(struct inode)); // read the inodes from myfs
    for(int i = 0; i < NUM_INODES; i++){ // iterate through the inodes, if its a file print 'File', if its a directory print 'Directory'
        if(inodes[i].used == 1 && inodes[i].dir == 0) printf("File: %s %d\n", inodes[i].name, inodes[i].size);
        else if(inodes[i].used == 1 && inodes[i].dir == 1) printf("Directory: %s %d\n", inodes[i].name, inodes[i].size);
    }
}

// ------------------------------- Main Function ------------------------------ //
int main(int argc, char* argv[]){
    printf("#----------------------- Initializing MYFS - File System -----------------------#\n");
    myfs = open("./myfs", O_RDWR);
    if(myfs == -1) myfs = init();
    // sleep(1); printf("Loading----------25%%\n"); sleep(1); printf("Loading------------------50%%\n"); sleep(1); printf("Loading--------------------------75%%\n"); sleep(1); printf("Loading----------------------------------100%%\n"); sleep(1); //Uncomment this line for kewl kewl loading effect
    printf("#----------------------- MYFS - File System Initialized  -----------------------#\n"); 
    // sleep(1);
    FILE *stream = fopen(argv[1], "r"); // open the input file stream -> 'r' for read mode
    if(stream == NULL){ // if the file doesn't exist, print error and exit
        printf("Error opening file\n"); exit(1);
    }
    // line is the buffer to store the input line, command is the buffer to store the commands and args to be executed, len is the length of the line
    char* line = NULL; char command[3]; size_t len = 0;

    while(getline(&line, &len, stream) != - 1){ // read the input file line by line until EOF reached
        sscanf(line, "%s %[^\n]", command, line); // split the line into command and args and check which command it is, then execute the corresponding function
        if(strcmp(command, "CR") == 0){
            char* filename = strtok(line, " ");
            int size = atoi(strtok(NULL, " "));
            CR(filename, size);
        }
        else if(strcmp(command, "DL") == 0) DL(line);
        else if(strcmp(command, "CP") == 0){
            char* srcname = strtok(line, " "), *dstname = strtok(NULL, " ");
            CP(srcname, dstname);
        }
        else if(strcmp(command, "MV") == 0){
            char* srcname = strtok(line, " "), *dstname = strtok(NULL, " ");
            MV(srcname, dstname);
        }
        else if(strcmp(command, "CD") == 0){
            line = strtok(line, "\n"); CD(line);
        }
        else if(strcmp(command, "DD") == 0) DD(line);
        else if(strcmp(command, "LL") == 0) LL();
    }
    free(line); fclose(stream); close(myfs); // free the line buffer, close the input file stream and close the file system
    printf("#----------------------- MYFS - File System Closed -----------------------#\n");
	return 0;
}