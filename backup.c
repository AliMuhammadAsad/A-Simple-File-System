#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h> // For using boolean values
#include<string.h> // For manipulating strings
#include<unistd.h> // low level file and directory handling/operations
#include<sys/types.h> // data types used by system calls and lib functions
#include<sys/stat.h> // defining file permissions and file status
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
 *
 *
 */


#define BLOCK_SIZE 1024
#define NUM_BLOCKS 128
#define NUM_INODES 16
#define FILENAME_MAXLEN 8
int myfs;

/* inode */

// Define data structures for inode and dirent
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
    int myfs = open("myfs", O_CREAT | O_RDWR, 0666); // create a file named myfs with read write enabled
    if (myfs == -1) {
        printf("Error: Cannot create file system .myfs\n");
        return -1;
    }

    ftruncate(myfs, BLOCK_SIZE * NUM_BLOCKS); // 128 * 1024 = 128KB allocated to myfs

    char fs_dbm_data[2] = {'A', 1};
    write(myfs, fs_dbm_data, 2); // writes 'A' and 1 to the first two bytes of myfs - identification and directory bitmap, 'A' for ident, 1 shows it is a directory

    // Initializing the Root Inode
    struct inode root_inode;
    root_inode.dir = 1; // root inode is a directory
    strcpy(root_inode.name, "/"); // first root directory named "/"
    root_inode.size = sizeof(struct dirent);
    root_inode.blockptrs[0] = 1;
    root_inode.used = 1; // yes it is in use
    root_inode.rsvd = 0; // no it is not reserved for future use
    
    lseek(myfs, NUM_BLOCKS, SEEK_SET);
    write(myfs, (char*)&root_inode, sizeof(struct inode));

    // Initializing the Root Directory Entry
    struct dirent root_dirent;
    strcpy(root_dirent.name, ".");
    root_dirent.namelen = 1;
    root_dirent.inode = 0;

    lseek(myfs, BLOCK_SIZE, SEEK_SET);
    write(myfs, (char*)&root_dirent, sizeof(struct dirent));

    // Initialize the Data Region
    char Data[BLOCK_SIZE];
    memset(Data, 0, BLOCK_SIZE);  // Fill data blocks with null characters
    for (int i = 1; i < NUM_BLOCKS; i++) write(myfs, (char*)&Data, BLOCK_SIZE);

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
    // Searches for available inodes and returns the index of the first available inode, if no inodes available, returns -1
    struct inode inodes[NUM_INODES];
    lseek(myfs, NUM_BLOCKS, SEEK_SET);
    read(myfs, (char*)&inodes, NUM_INODES * sizeof(struct inode));
    for(int i = 0; i < NUM_INODES; i++){
        if(inodes[i].used == 0) return i;
    }
    printf("Error: No available inodes\n");
    return -1;
}

int findAvailableDataBlock(int* blockpointers, int blockcount){
    // Searches for available data blocks, returns the index of the first available data block, if no data blocks available, returns -1
    int ib = 1;
    char occupado[BLOCK_SIZE];
    lseek(myfs, 0, SEEK_SET);
    read(myfs, occupado, BLOCK_SIZE);

    for(int i = 0; i < blockcount; i++){
        while(ib <= BLOCK_SIZE){
            if(occupado[ib] != (char)1){
                blockpointers[i] = ib; ib++;
                break;
            }
            ib++;
        }
        if(ib > BLOCK_SIZE){
            printf("Error: No available data blocks\n"); return -1;
        }
    }
    return 0;
}

int findParentInode(char* filename){
    // Returns the inode of the parent directory of the file
    if(strcmp(filename, "/") == 0){
        printf("Error: Root directory is invalid\n"); return -1;
    }
    char directory[1024]; int directory_inode = 0;
    struct inode parent_inode; struct dirent curr_entry;
    char restpath[strlen(filename) + 1]; strcpy(restpath, filename);
    
    while(scanf(restpath, "/%[^/]%s", directory, restpath) == 2){
        lseek(myfs, BLOCK_SIZE + directory_inode * sizeof(struct inode), SEEK_SET);
        read(myfs, (char*)&parent_inode, sizeof(struct inode));
        int dirsize = parent_inode.size;
        int found = 0;
        for(int i = 0; i < dirsize; i++){
            lseek(myfs, BLOCK_SIZE * parent_inode.blockptrs[0] + i, SEEK_SET);
            read(myfs, (char*)&curr_entry, sizeof(struct dirent));
            if(strcmp(curr_entry.name, directory) == 0){
                lseek(myfs, BLOCK_SIZE + curr_entry.inode * sizeof(struct inode), SEEK_SET);
                read(myfs, (char*)&parent_inode, sizeof(struct inode));
                if(parent_inode.dir == 1){
                    found = 1; 
                    directory_inode = curr_entry.inode;
                    break;
                }
            }
        }
        if(found == 0){
            printf("Error: No such directory %s in the path\n", directory); return -1;
        }
    }
    return directory_inode;
}

// ------------------------------ Create File ------------------------------ //

int CR(char* filename, int size){
    struct inode finode;
    int directory_inode = findParentInode(filename);
    if(directory_inode == -1) return -1;

    int blockcount = (size % BLOCK_SIZE != 0) + (size / BLOCK_SIZE);
    if(blockcount > NUM_BLOCKS){
        printf("Error: File size exceeds maximum file size\n"); return -1;
    }

    int available_block = findAvailableDataBlock(finode.blockptrs, blockcount);
    if(available_block == -1) return -1;

    int available_inode = findAvailableInode();
    if(available_inode == -1) return -1;
    
    finode.dir = 0; // 0 since its a file, not a directory
    strcpy(finode.name, filename); // set the name of the file
    finode.size = size; // set its size
    finode.used = 1; // yes it is in use
    finode.rsvd = 0; // no it is not reserved for future use

    lseek(myfs, NUM_BLOCKS + available_inode * sizeof(struct inode), SEEK_SET);
    write(myfs, (char*)&finode, sizeof(struct inode));

    char c = (char)1;
    char data[BLOCK_SIZE];
    int buffsize = BLOCK_SIZE;
    for(int i = 0; i < blockcount; i++){
        lseek(myfs, finode.blockptrs[i], SEEK_SET);
        write(myfs, (char*)&c, 1);

        if(size > BLOCK_SIZE) size -= BLOCK_SIZE;
        else buffsize = size;

        for(int j = 0; j < buffsize; j++) data[j] = (char)(97 + (rand() % 26));
        
        lseek(myfs, BLOCK_SIZE * finode.blockptrs[i], SEEK_SET);
        write(myfs, data, buffsize);
    }
    return 0;
}

// ------------------------------ Delete File ------------------------------ //
int DL(char* filename){
    return 0;
}

// ------------------------------ Copy File ------------------------------ //

int CP(char* srcname, char* dstname){
    return 0;
}

// ------------------------------ Move File ------------------------------ //

int MV(char* srcname, char* dstname){
    return 0;
}

// ------------------------------ Create Directory ------------------------------ //

int CD(char* dirname){
    return 0;
}

// ------------------------------ Delete Directory ------------------------------ //

int DD(char* dirname){
    return 0;
}

// ------------------------------ List all Files ------------------------------ //

void LL(){
    struct inode inodes[NUM_INODES];
    lseek(myfs, NUM_BLOCKS, SEEK_SET);
    read(myfs, inodes, 16*56);
    for(int i = 0; i < NUM_INODES; i++){
        if(inodes[i].used == 1 && inodes[i].dir == 0) printf("File: %s %d\n", inodes[i].name, inodes[i].size);
        else if(inodes[i].used == 1 && inodes[i].dir == 1) printf("Directory: %s %d\n", inodes[i].name, inodes[i].size);
    }
}


// ------------------------------- Main Function ------------------------------ //
int main (int argc, char* argv[]) {
    myfs = open("./myfs", O_RDWR);
    if(myfs == -1) myfs = init();

    FILE *stream = fopen(argv[1], "r");
    if(stream == NULL) {
        printf("Error opening file\n");
        exit(1);
    }

    char* line = NULL; char command[3]; size_t len = 0;

    while(getline(&line, &len, stream) != - 1){
        sscanf(line, "%s %[^\n]", command, line);
        if(strcmp(command, "CR") == 0){
            char* filename = strtok(line, " ");
            int size = atoi(strtok(NULL, " "));
            CR(filename, size);
        }
        else if(strcmp(command, "DL") == 0){
            DL(line);
        }
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
        else if(strcmp(command, "DD") == 0){
            DD(line);
        }
        else if(strcmp(command, "LL") == 0){
            LL();
        }
    }
    free(line);
    fclose(stream);
    close(myfs);
	return 0;
}