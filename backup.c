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
    // searches for the given path/filename/dirname then writes it into a directory if not found
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
    struct inode root_inode; struct dirent root_dirent;

    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
    int size = root_inode.size, val = -1, i = 0; *block = root_inode.blockptrs[0];
    while(i < size){
        lseek(myfs, BLOCK_SIZE * (*block) + i, SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));
        if(strcmp(root_dirent.name, filename) == 0){
            val = -2; *finode = root_dirent.inode;
            struct inode root_inode;
            lseek(myfs, NUM_BLOCKS + root_dirent.inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
            if(root_inode.dir == dir) return i;
        }
        i += sizeof(struct dirent);
    }
    return val;
}

int execution(int directory_inode, int directory_entry){
    struct inode root_inode; struct dirent root_dirent;

    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));
    int size = root_inode.size, blockOff = root_inode.blockptrs[0];

    if(directory_entry != size - sizeof(struct dirent)){
        lseek(myfs, BLOCK_SIZE * blockOff + size - sizeof(struct dirent), SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));

        lseek(myfs, BLOCK_SIZE * blockOff + directory_entry, SEEK_SET); write(myfs, (char*)&root_dirent, sizeof(struct dirent));
    }

    root_inode.size = size - sizeof(struct dirent);
    lseek(myfs, NUM_BLOCKS + directory_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));

    return 0;
}

void successiveExecution(int finode){
    struct inode root_inode;
	lseek(myfs, NUM_BLOCKS + finode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(inode));

	root_inode.used = 0;
	lseek(myfs, NUM_BLOCKS + finode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));
	
	char nc = '\0', blockData[BLOCK_SIZE];
	for (int i = 0; i < BLOCK_SIZE; i++) blockData[i] = nc;

	if(root_inode.dir == 0){
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
	else{
		lseek(myfs, root_inode.blockptrs[0], SEEK_SET); write(myfs, &nc, 1);

		struct dirent root_dirent;
		for(int i = 0; i < root_inode.size; i += sizeof(struct dirent)){
			lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[0], SEEK_SET); read(myfs, &root_dirent, sizeof(struct dirent));
			successiveExecution(root_dirent.inode);
		}
		lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[0], SEEK_SET); write(myfs, &blockData, BLOCK_SIZE);
    }
}

// ------------------------------ Create File ------------------------------ //

int CR(char* filename, int size){
    struct inode finode;
    
    int directory_inode = findParentInode(filename); // find the inode of the parent directory where the file will be created
    if(directory_inode == -1) return -1;

    int blockcount = (size % BLOCK_SIZE != 0) + (size / BLOCK_SIZE); // number of blocks needed to store the file
    if(blockcount > 8){
        printf("Filesize exceeding size limit\n"); return -1;
    }
    
    if(findAvailableDataBlock(finode.blockptrs, blockcount) == -1) return -1;

    int available_inode = findAvailableInode(); // find available inode
    if(available_inode == -1) return -1;

    if(assassin(filename, directory_inode, available_inode, 0) == -1) return -1;

    finode.dir = 0; // 0 since its a file, not a directory
    strcpy(finode.name, filename); // set the name of the file
    finode.size = size; // set its size
    finode.used = 1; // yes it is in use
    finode.rsvd = 0; // no it is not reserved for future use

    // write the inode of the file into myfs
    lseek(myfs, NUM_BLOCKS + available_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&finode, sizeof(struct inode));

    char c = (char)1, data[BLOCK_SIZE];
    int buffsize = BLOCK_SIZE;
    for(int i = 0; i < blockcount; i++){
        // mark the data block as occupied
        lseek(myfs, finode.blockptrs[i], SEEK_SET); write(myfs, &c, 1);

        if(size > BLOCK_SIZE) size -= BLOCK_SIZE;
        else buffsize = size;

        for(int j = 0; j < buffsize; j++) data[j] = (char)(97 + (rand() % 26)); // random data for the file
        // write the data into the data block
        lseek(myfs, BLOCK_SIZE * finode.blockptrs[i], SEEK_SET); write(myfs, data, buffsize);
    }
    printf("File '%s' created successfully\n", filename);
    return 0;
}

// ------------------------------ Delete File ------------------------------ //

int DL(char* filename){
    int directory_inode = findParentInode(filename);
    if(directory_inode == -1) return -1;

    int block, finode;
    int t_inode = stalker(filename, &block, &finode, directory_inode, 0);
    if(t_inode < 0){
        printf("Error: File '%s' does not exist\n", filename); return -1;
    }

    execution(directory_inode, t_inode); successiveExecution(finode);
    printf("File '%s' deleted successfully\n", filename);
    return 0;
}

// ------------------------------ Copy File ------------------------------ //
int CP(char *srcname, char *dstname){
    int src_inode = findParentInode(srcname), dst_inode = findParentInode(dstname);
    if(src_inode == -1 || dst_inode == -1) return -1;

    int block_og, block_cp, finode_og, finode_cp;
    int source_entry = stalker(srcname, &block_og, &finode_og, src_inode, 0), directory_entry = stalker(dstname, &block_cp, &finode_cp, dst_inode, 0);
    if(source_entry < 0){
        printf("Error: File '%s' does not exist, or you've provided a directory - can't handle directories\n", srcname); return -1;
    }

    struct inode root_inode;
    lseek(myfs, NUM_BLOCKS + finode_og * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&root_inode, sizeof(struct inode));

    int blockcount = (root_inode.size % BLOCK_SIZE != 0) + (root_inode.size / BLOCK_SIZE);
    int blockData[blockcount];
    for(int i = 0; i < blockcount; i++) blockData[i] = root_inode.blockptrs[i];

    strcpy(root_inode.name, dstname); 
    
    if(findAvailableDataBlock(root_inode.blockptrs, blockcount) == -1) return -1;
    
    int available_inode = findAvailableInode();
    if(available_inode == -1) return -1;

    if(directory_entry > -1){
        struct dirent root_dirent;
        lseek(myfs, BLOCK_SIZE * block_cp + directory_entry, SEEK_SET); read(myfs, (char*)&root_dirent, sizeof(struct dirent));

        successiveExecution(root_dirent.inode); execution(dst_inode, directory_entry);
    }

    lseek(myfs, NUM_BLOCKS + available_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&root_inode, sizeof(struct inode));

    char temp_data[BLOCK_SIZE];
    for(int i = 0; i < blockcount; i++){
        lseek(myfs, BLOCK_SIZE * blockData[i], SEEK_SET); read(myfs, temp_data, BLOCK_SIZE);

        lseek(myfs, BLOCK_SIZE * root_inode.blockptrs[i], SEEK_SET); write(myfs, temp_data, BLOCK_SIZE);
    }

    struct inode temp_inode; struct dirent temp_dirent;

    lseek(myfs, NUM_BLOCKS + dst_inode * sizeof(struct inode), SEEK_SET); read(myfs, (char*)&temp_inode, sizeof(struct inode));

    int size = temp_inode.size, blockOff = temp_inode.blockptrs[0];

    strcpy(temp_dirent.name, dstname);
    temp_dirent.namelen = strlen(dstname); temp_dirent.inode = available_inode;

    lseek(myfs, BLOCK_SIZE * blockOff + size, SEEK_SET); write(myfs, (char*)&temp_dirent, sizeof(struct dirent));

    temp_inode.size = size + sizeof(struct dirent);
    lseek(myfs, NUM_BLOCKS + dst_inode * sizeof(struct inode), SEEK_SET); write(myfs, (char*)&temp_inode, sizeof(struct inode));
    printf("File '%s' copied successfully to destination '%s' \n", srcname, dstname);
    return 0;
}


// ------------------------------ Move File ------------------------------ //

int MV(char* srcname, char* dstname){
    CP(srcname, dstname); DL(srcname); printf("File '%s' moved successfully to destination '%s' \n", srcname, dstname);
    return 0;
}

// ------------------------------ Create Directory ------------------------------ //

int CD(char* dirname){
    int parent_inode = findParentInode(dirname);
    if(parent_inode == -1) return -1; // Return an error if parent directory doesn't exist
    
    int block; 
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

int DD(char* dirname){
    int directory_inode = findParentInode(dirname);
    if(directory_inode == -1) return -1;

    int block, finode;
    int t_inode = stalker(dirname, &block, &finode, directory_inode, 1);
    if(t_inode < 0){
        printf("Error: Directory '%s' does not exist\n", dirname); return -1;
    }
    execution(directory_inode, t_inode); successiveExecution(finode);
    printf("Directory '%s' deleted successfully\n", dirname);
    return 0;
}

// ------------------------------ List all Files ------------------------------ //

void LL(){
    printf("\n\nMYFS has the following files and directories stored in the system:\n");
    struct inode inodes[NUM_INODES];
    lseek(myfs, NUM_BLOCKS, SEEK_SET); read(myfs, inodes, NUM_INODES * sizeof(struct inode));
    for(int i = 0; i < NUM_INODES; i++){
        if(inodes[i].used == 1 && inodes[i].dir == 0) printf("File: %s %d\n", inodes[i].name, inodes[i].size);
        else if(inodes[i].used == 1 && inodes[i].dir == 1) printf("Directory: %s %d\n", inodes[i].name, inodes[i].size);
    }
}

// ------------------------------- Main Function ------------------------------ //
int main(int argc, char* argv[]){
    printf("#----------------------- Initializing MYFS - File System -----------------------#\n");
    myfs = open("./myfs", O_RDWR);
    if(myfs == -1) myfs = init();
    printf("#----------------------- MYFS - File System Initialized -----------------------#\n");
    FILE *stream = fopen(argv[1], "r");
    if(stream == NULL) {
        printf("Error opening file\n"); exit(1);
    }

    char* line = NULL; char command[3]; size_t len = 0;

    while(getline(&line, &len, stream) != - 1){
        sscanf(line, "%s %[^\n]", command, line);
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
    free(line); fclose(stream); close(myfs);
    printf("#----------------------- MYFS - File System Closed -----------------------#\n");
	return 0;
}