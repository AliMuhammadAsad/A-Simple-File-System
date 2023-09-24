# A Simple(UNIX Based) File System

This program was made on receiving the homework assignment for the course CS232 Operating Systems. The program would create a snapshot of the file system in a file ```myfs``` (my file system), having support for some simple commands. Since a snapshot is being created, the ```myfs``` would retain any data written onto it - simulating an actual hard disk.

## Compilation:
The accompanying ```makefile``` has commands for compiling the C file and running the output file. 

It can also be compiled by ```gcc filesystem.c -o myfs.out``` and run using ```./myfs.out sampleinput.txt```. If you want to test it with any other file, then simple replace the ```sampleinput.txt``` file with your filename. 

### 1. Introduction
The assignment was to implement simulate a simple file system as follows:
<ol>
    <li>The whole disk is 128KB in size.</li>
    <li>The top most directory is the root directory (/)</li>
    <li>The system can have a maximum of 16 files/directories</li>
    <li>A file can have a maximum of 8 blocks (no indirect pointers). Each block is 1 KB in size.</li>
    <li>A file/directory name can be of 8 chars max (including NULL char). There can be only one file of a given name in a directory.</li>
</ol>

### 2. Disk Layout
The disk has 128 blocks, divided into 1 super block, and 127 data blocks. The superblock contains the 128 byte free block list where each byte contains a boolean value indicating whether that particular block is free or not. Just after the free block list, in the super block, we have the inode table containing the 16 inodes themselves. Each inode is 56 bytes in size and contains metadata about the stored files/directories. It can also be seen below:

```
 ___ ___ ___ ___ ___ ___ ___ ___ ___ ___ ___ 
|   |   |   |   |                       |   |
| 0 | 1 | 2 | 3 |     .....             |127|
|___|___|___|___|_______________________|___|
|   \    <-----  data blocks ------>
|     \
|       \
|         \
|           \
|             \
|               \
|                 \
|                   \
|                     \
|                       \
|                         \
|                           \
|                             \
|                               \
|                                 \
|                                   \
|                                     \
|                                       \
|                                         \
|     <--- super block --->                \
|___________________________________________|
|               |      |      |     |       |
|     free      |      |      |     |       |
|     block     |inode0|inode1|.... |inode15|
|     list      |      |      |     |       |
|_______________|______|______|_____|_______|
```

### 3. Supporting Commands:
##### 3.1 Create a file
syntax: CR filename size
Creates a file with name 'filename' of given size - filename is an absolute path 

##### 3.2 Delete a file
syntax: DL filename
Deletes the file called 'filename'.

##### 3.3 Copy a File
syntax: CP srcname dstname
Copies a file titled 'srcname' to a file titled 'dstname'.

##### 3.4 Move a file
syntax: MV srcname dstname
Moves a file 'srcname' to file 'dstname'.

##### 3.5 Create a Directory
syntax: CD dirname
Create an empty directory at the path indicated by 'dirname' where 'dirname' is an absolute path.

##### 3.6 Remove a Directory
syntax: DD dirname
Removes the directory at the path indicated by dirname

##### 3.7 List all Files
syntax: LL
Lists all files/directories on the hard disk along with their sizes. Each file/directory on a separate line.