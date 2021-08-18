#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
// A working file system with comments explaining the source code
// Dylan Wagner circa 2019
//
//

struct inode { //the given inode 
  char name[16];
  int size;
  int blockPointers[8];
  int used;
};

struct fs_t { //our structure (I regret the long name FilePointer)
  int filePointer;
};

// open the file with the above name
void fs_open(struct fs_t* fs, char diskName[16]) {
   // this file will act as the "disk" for your file system
   fs->filePointer = open(diskName, O_RDWR); //open the file with RDWR
   if (fs->filePointer < 0) 
   {
     printf("could not open %s\n", diskName); //if the file didn't 
   }


}

// close and clean up all associated things
void fs_close(struct fs_t* fs) {
   if (fs->filePointer < 0) { //can't close twice without open
     return;
   }
   close(fs->filePointer);//close the filepointer
   fs->filePointer = -1;//so we can't close twice
}

//a helper method to cut down on my copy pasting habit
// this finds a node index from a given name and returns the index number
int fs_find_inode_idx_from_name(struct fs_t* fs, char name[16]) {
  lseek(fs->filePointer, 128, SEEK_SET);
  struct inode node;
  for (int i = 0; i < 16; ++i) {
    read(fs->filePointer, &node, sizeof(node));
    if (node.used && !strcmp(node.name, name)) {
      return i;
    }
  }
  return -1;
}

//another helper method which takes an index and reads the inode at index into node
void inode_from_idx(struct fs_t* fs, int idx, struct inode* node) {
  lseek(fs->filePointer, 128 + idx * 56, SEEK_SET);
  read(fs->filePointer, node, 56);
}

//create a file with this name and this size
void fs_create(struct fs_t* fs, char name[16], int size) {
  if (size > 8) { //if there are more than 8 block pointers there's too many
    printf("ERROR: file size too large (too many blocks)\n");
    return;
  }
  char freeList[128]; //create a charlist of 128 
  lseek(fs->filePointer, 0, SEEK_SET); //set the filepointer to the beginning
  read(fs->filePointer, freeList, 128); //read the first 128 characters into freelist
  struct inode node; //create a inode
  memset(&node, 0, 56); //clear it
  for (int i = 1; i < 128; ++i) { //scan through all freeblock characters
    if (freeList[i] == 0) { //if our free list is empty
      freeList[i] = 1; //use it for our node blockpointer
      node.blockPointers[node.size] = i;
      if (size == ++node.size) { //if we've used the size of our node pointers
        break; // break out
      }
    }
  }
  if (node.size != size) { //check if we have enough space 
    printf("ERROR: not enough space\n");
    // not enough space
    return;
  }

  struct inode temp; //create a temporary node
  int idx = fs_find_inode_idx_from_name(fs, name); //get the index of our name, if it exists 
  if (idx >= 0) {
    printf("ERROR: duplicate filename\n"); //we can't have duplicate names
    return;
  }

  lseek(fs->filePointer, 128, SEEK_SET); //set filepointer to the 129th bit
  for (int i = 0; i < 16; ++i) { //traverse all nodes
    read(fs->filePointer, &temp, 56); //read the node(size 56) into temp
    if (!temp.used) { //if temp is not used
      node.used = 1; //it is now!
      strncpy(node.name, name, 16); //copy the name into node name (max 16 characters)
      lseek(fs->filePointer, 128 + i * 56, SEEK_SET); //lseek to the current bit
      write(fs->filePointer, &node, sizeof(node)); //write node into the block
      lseek(fs->filePointer, 0, SEEK_SET); //seek back to the beginning
      write(fs->filePointer, freeList, 128); //update our freelist
      return;
    }
  }

  
  return;
}

// Delete the file with this name
void fs_delete(struct fs_t* fs, char name[16]) {
  lseek(fs->filePointer, 128, SEEK_SET); //start looking at the 129th bit
  int idx = fs_find_inode_idx_from_name(fs, name); //find the index of the given name
  if (idx < 0) { //if the index doesn't exists
    printf("File doesn't exist!\n");
    return;
  }
  struct inode temp; //temporary node
  inode_from_idx(fs, idx, &temp); //put node at index into temp

  
  char freeList[128]; //create a empty free list
  lseek(fs->filePointer, 0, SEEK_SET); //put filepointer to beginning of freelist
  read(fs->filePointer, freeList, 128); //read first 128 characters into freeList (basically copy)

  for (int i = 0; i < temp.size; ++i) { //set our nodes blockpointers to be free
    freeList[temp.blockPointers[i]] = 0;
  }
  temp.used = 0; //temp is now free
  lseek(fs->filePointer, 0, SEEK_SET);//set filepointer to start
  write(fs->filePointer, freeList, 128); //copy freelist over
  lseek(fs->filePointer, idx * 56, SEEK_CUR); //return to where temp node was stored
  write(fs->filePointer, &temp, 56); //overwrite with new temp node
}

// List names of all files on disk
void fs_ls(struct fs_t* fs) {
  
  struct inode temp; //create temp inode
  lseek(fs->filePointer, 128, SEEK_SET); //go to 129th bit
  for (int i = 0; i < 16; ++i) { //go through the inodes
    read(fs->filePointer, &temp, sizeof(temp)); //each for loop check the node
    if (temp.used) { //if it's in use print it and it's size
      printf("%16s %6dB\n", temp.name, temp.size * 1024);
    }
  }
}


// read this block from this file
void fs_read(struct fs_t* fs, char name[16], int blockNum, char buf[1024]) {
  int idx = fs_find_inode_idx_from_name(fs, name); //find the index of the node we want to read
  if (idx < 0) { //if it doesn't match
    printf("file not found\n");
    return;
  }
  struct inode test; //create node
  inode_from_idx(fs, idx, &test);
  if (blockNum >= test.size) {
    printf("BlockNum is too large!");
    return;
  }
  lseek(fs->filePointer, test.blockPointers[blockNum] * 1024, SEEK_SET); //go to the bit with our nodes blockPointers
  read(fs->filePointer, buf, 1024); //read
}

// write this block to this file
void fs_write(struct fs_t* fs, char name[16], int blockNum, char buf[1024]) {
 int idx = fs_find_inode_idx_from_name(fs, name); //find the index of the node we want to write to 
  if (idx < 0) { //file not found
    printf("file not found\n");
    return;
  }
  struct inode test; //create inode 
  inode_from_idx(fs, idx, &test);
  if (blockNum >= test.size) { 
    printf("BlockNum is too large!");
    return;
  }
  lseek(fs->filePointer, test.blockPointers[blockNum] * 1024, SEEK_SET); //go to the bit with our nodes blockPointers to write at 
  write(fs->filePointer, buf, 1024); //write
}

void fs_repl() {
}