/* Program 4 source code */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prog4.h"

/* global variables to make life easier */
int seq, mask;
const char dot[60] = "."; /* current directory */
const char dotdot[60] = ".."; /* parent directory */
int FDInode;
int FDIndex;
int FDOffset;

int openFD[MAX_OPEN_FILES];

/* Some useful macros */

#define abort(s) { printf ("%s, aborting\n", s); exit (1); }
#define warning(s) { printf ("warning: %s\n", s); }
#define ret_err(s) { printf ("error: %s\n", s); return -1; }
#define void_err(s) { printf ("error: %s\n", s); return; }

/* open files are kept in a global table */

struct open_file { /* in-memory information about open files */
};

struct SuperBlock {
    int builtFS;
    int inodeSum;
    int inodeBitmap;
    int inodeBPAddr;
    int inodeStartAddr;
    int inodeFree;
    int fileMax;
    int blockSize;
    int blockSum;
    int blockBitmap;
    int blockBPAddr;
    int blockFree;
    int blockFreeSize;
    int rootDir;
    int rest[242];
};

struct Inode {
    int addr[6];
    int firstLevel;
    int secondLevel;
};

struct FirstBlock {
    int addr[256];
};

struct SecondBlock1 {    // point to the block number in disc
    int addr[256];
};

struct SecondBlock2 {
    int addr[256];
};

struct DirElement {
    int inodeNum;
    char filename[60];
};

struct Dir {
    int existNum;
    struct DirElement current;
    struct DirElement parent;
    struct DirElement files[13];

    int rest[15];
};
#define INODE_MAX 10024
#define INODE_START 34
#define MAX_FILE_LENGTH (200 * 1024 * 1024)
#define BLOCK_MAX 2000000
#define BITMAP_OFFSET 200
#define INT_SIZE sizeof(int)
#define BITMAP_SIZE (1024*8)
#define INODE_SIZE sizeof(struct Inode)
#define INODE_PER_BLOCK (BLOCKSIZE/INODE_SIZE)
#define INODE_ARRAR_SIZE (8+256+256*257+1)

int inodeArray[INODE_ARRAR_SIZE];
struct Bitmap {
    int bitmap[BLOCKSIZE / INT_SIZE];
};

struct InodeArray {
    struct Inode inodes[INODE_PER_BLOCK];
};

/**
 * Helper function
 */
void init_bitmap_helper(int startAddr, int len, int blockBpm);
void init_inode_helper(int startAddr, int len, int blockBpm);
void init_root_dir_helper(int addr, int bitmapAddr);
void set_bitmap_helper(int bitNum, int blockAddr);
int test_bitmap_helper(int bitNum, int blockAddr);
void clear_bitmap_helper(int bitNum, int blockAddr);
int parse_path_helper(char *path, char subPath[10][60]);
void init_dir_helper(struct Dir *dirTmp, int iNode);
int find_free_helper(int beginner, int blockAddr);
void add_dir_helper(char pathName[60], int parrentDirBlock);
void rm_dir_helper(char pathName[60], int parentDirBlock, int currentDirBlock);
int inode_to_block_helper(int inodeNumber);
void creat_inode_helper(int inodeNumber, int blockNumber);
int dir_block_find_helper(char pathName[60], int blockNumber);
int dir_inode_find_helper(char pathName[60], int blockNumber);
void clear_parent_dir_helper(int deleteInode, int parentDirBlock);
void add_parent_dir_helper(int addInode, char pathName[60], int parentDirBlock);
int add_file_helper(char pathName[60], int parentDirBlock);
void rm_file_helper(int curentInode, int parentDirBlock);
void read_all_block_helper(int inode);
void save_all_blocks_helper(int inode);

/* The API */

/* open an existing file for reading or writing */
int my_open(const char * path) {
    int i = 0;
    char subPath[10][60];
    int pathLevel = parse_path_helper((char*) path, subPath);
    int parDirBlock = BITMAP_OFFSET;
    for (i = 0; i < pathLevel - 1; i++) {
        parDirBlock = dir_block_find_helper(subPath[i], parDirBlock);
        if (parDirBlock == -1)
            return -1;
    }
    int isExist = dir_inode_find_helper(subPath[i], parDirBlock);
    if (isExist) {
        FDInode = isExist;
        FDIndex = 0;
        FDOffset = 0;
        for (i = 0; i < MAX_OPEN_FILES; i++) {
            if (openFD[i] == 0) {
                openFD[i] = isExist;
                break;
            }
        }
    }
    return isExist;
}

/* open a new file for writing only */
int my_creat(const char * path) {
    int i = 0;
    char subPath[10][60];
    int pathLevel = parse_path_helper((char*) path, subPath);
    int parentDirBlock = BITMAP_OFFSET;

    for (i = 0; i < pathLevel - 1; i++) {
        parentDirBlock = dir_block_find_helper(subPath[i], parentDirBlock);
        if (parentDirBlock == -1)
            return -1;
    }
    int isExist = dir_block_find_helper(subPath[i], parentDirBlock);
    if (isExist > 0)
        return -1;
    int inodeAdd = add_file_helper(subPath[i], parentDirBlock);
    FDInode = inodeAdd;
    FDIndex = 0;
    FDOffset = 0;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFD[i] == 0) {
            openFD[i] = inodeAdd;
            break;
        }
    }
    return inodeAdd;
}

/* sequentially read from a file */
int my_read(int fd, void * buf, int count) {
    int result = 0;
    char * tmp = buf;

    if (FDInode != fd)
        return -1;
    char readUnit[BLOCKSIZE];
    read_all_block_helper(fd);

    while (count > 0) {
        if (FDIndex == 6 || FDIndex == 263
                || (FDIndex - 264 >= 0 && (FDIndex - 264) % 257 == 0)) {
            FDIndex++;
            continue;
        }
        read_block(inodeArray[FDIndex], readUnit);

        if (count < (BLOCKSIZE - FDOffset)) {
            memcpy(tmp + result, readUnit + FDOffset, count);
            result += count;
            count -= count;
            FDOffset += count;
        } else {
            memcpy(tmp + result, readUnit + FDOffset, BLOCKSIZE - FDOffset);
            count -= (BLOCKSIZE - FDOffset);
            result += (BLOCKSIZE - FDOffset);
            FDIndex++;
            FDOffset = 0;
        }
    }
    return result;
}

/* sequentially write to a file */
int my_write(int fd, const void * buf, int count) {
    int result = 0;
    char *tmp = (char*) buf;
    if (FDInode != fd)
        return -1;

    char buffSup[BLOCKSIZE];
    read_block(0, buffSup);
    struct SuperBlock *readSuperBlock = (struct SuperBlock*) buffSup;
    if (readSuperBlock->blockFreeSize <= count / BLOCKSIZE)
        return -1;
    int bAddr = readSuperBlock->blockBPAddr;
    int bFree = readSuperBlock->blockFree;

    read_all_block_helper(fd);

    char writeUnit[BLOCKSIZE];
    while (count > 0) {
        if (inodeArray[FDIndex] == 0) {
            inodeArray[FDIndex] = bFree;
            set_bitmap_helper(bFree, bAddr);
            bFree = find_free_helper(bFree, bAddr);
            readSuperBlock->blockFree = bFree;
            readSuperBlock->blockFreeSize--;

            if (FDIndex == 6) {
                struct FirstBlock fir;
                write_block(inodeArray[FDIndex], (char *) &fir);
                FDIndex++;
                continue;
            } else if (FDIndex == 263) {
                struct SecondBlock1 sec1;
                write_block(inodeArray[FDIndex], (char *) &sec1);
                FDIndex++;
                continue;
            } else if (FDIndex - 264 >= 0 && (FDIndex - 264) % 257 == 0) {
                struct SecondBlock2 sec2;
                write_block(inodeArray[FDIndex], (char *) &sec2);
                FDIndex++;
                continue;
            }
        }
        read_block(inodeArray[FDIndex], writeUnit);
        if (count < (BLOCKSIZE - FDOffset)) {
            memcpy(writeUnit + FDOffset, tmp + result, count);
            write_block(inodeArray[FDIndex], writeUnit);
            result += count;
            FDOffset += count;
            count -= count;
        } else {
            memcpy(writeUnit + FDOffset, tmp + result, BLOCKSIZE - FDOffset);
            write_block(inodeArray[FDIndex], writeUnit);
            count -= (BLOCKSIZE - FDOffset);
            result += (BLOCKSIZE - FDOffset);
            FDIndex++;
            FDOffset = 0;
        }
    }

    write_block(0, buffSup);
    save_all_blocks_helper(fd);
    return result;
}

int my_close(int fd) {
    int i;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFD[i] == fd) {
            FDInode = 0;
            FDIndex = 0;
            FDOffset = 0;
            openFD[i] = 0;
            return 0;
        }
    }
    return -1;
}

int my_remove(const char * path) {
    int i = 0;
    char subPath[10][60];
    int pathLevel = parse_path_helper((char*)path, subPath);
    int parentDirBlock = BITMAP_OFFSET;

    for (i = 0; i < pathLevel - 1; i++) {
        parentDirBlock = dir_block_find_helper(subPath[i], parentDirBlock);
        if (parentDirBlock == -1)
            return -1;
    }

    // test whether the file already exist
    int inode = dir_inode_find_helper(subPath[i], parentDirBlock);
    if (inode < 0)
        return -1;
    rm_file_helper(inode, parentDirBlock);
    return 0;
}

int my_rename(const char * old, const char * new) {
    int i = 0;
    char pathOld[10][60], pathNew[10][60];
    int pathLevelOld = parse_path_helper((char*)old, pathOld);
    int pathLevelNew = parse_path_helper((char*)new, pathNew);
    int parrentDirBlock = BITMAP_OFFSET;
    for (i = 0; i < pathLevelOld - 1; i++) {
        parrentDirBlock = dir_block_find_helper(pathOld[i], parrentDirBlock);
        if (parrentDirBlock == -1)
            return -1;
    }
    int iNode = dir_inode_find_helper(pathOld[i], parrentDirBlock);
    if (iNode == -1)
        return -1;
    clear_parent_dir_helper(iNode, parrentDirBlock);
    parrentDirBlock = BITMAP_OFFSET;
    for (i = 0; i < pathLevelNew - 1; i++) {
        parrentDirBlock = dir_block_find_helper(pathNew[i], parrentDirBlock);
        if (parrentDirBlock == -1)
            return -1;
    }
    add_parent_dir_helper(iNode, pathNew[i], parrentDirBlock);
    return 0;
}

/* only works if all but the last component of the path already exists */
int my_mkdir(const char * path) {
    int i = 0;
    char subPath[10][60];
    int pathLevel = parse_path_helper((char*)path, subPath);
    int parentDirBlock = BITMAP_OFFSET;
    for (i = 0; i < pathLevel - 1; i++) {
        parentDirBlock = dir_block_find_helper(subPath[i], parentDirBlock);
        if (parentDirBlock == -1)
            return -1;
    }
    int isExist = dir_block_find_helper(subPath[i], parentDirBlock);
    if (isExist > 0)
        return -1;

    add_dir_helper(subPath[i], parentDirBlock);
    return 0;
}

int my_rmdir(const char * path) {
    int i = 0;
    char subPath[10][60];
    int pathLevel = parse_path_helper((char*)path, subPath);
    int parentDirBlock = BITMAP_OFFSET;
    for (i = 0; i < pathLevel - 1; i++) {
        parentDirBlock = dir_block_find_helper(subPath[i], parentDirBlock);
        if (parentDirBlock == -1)
            return -1;
    }
    int isExist = dir_block_find_helper(subPath[i], parentDirBlock);
    if (isExist < 0)
        return -1;
    rm_dir_helper(subPath[i], parentDirBlock, isExist);
    return 0;
}

/* Check to see if the device already has a file system on it,
 * and if not, create one. */
/* almost any error here should be fatal */

void my_mkfs() {

    struct SuperBlock superblock;
    superblock.builtFS = 1;
    superblock.inodeSum = INODE_MAX;
    superblock.inodeBitmap = 1;
    superblock.inodeBPAddr = 1;
    superblock.inodeStartAddr = INODE_START;
    superblock.inodeFree = 1;
    superblock.fileMax = MAX_FILE_LENGTH;
    superblock.blockSize = 1024;
    superblock.blockSum = BLOCK_MAX;
    superblock.blockBitmap = 32;
    superblock.blockBPAddr = 2;
    superblock.blockFree = BITMAP_OFFSET + 1;
    superblock.blockFreeSize = BLOCK_MAX - BITMAP_OFFSET - 1;
    superblock.rootDir = BITMAP_OFFSET;

    memset(openFD, 0, sizeof(openFD));
    char buff[BLOCKSIZE];
    int dev = dev_open();
    if (dev > 0) {
        read_block(0, buff);
        struct SuperBlock *read_spb = (struct SuperBlock *)buff;
        if (read_spb->builtFS != 1) { /* file system doesn't exist */
            write_block(0, (char *) &superblock);
            init_bitmap_helper(superblock.blockBPAddr, superblock.blockBitmap,
                    superblock.blockBPAddr);
            set_bitmap_helper(0, superblock.blockBPAddr);
            init_bitmap_helper(superblock.inodeBPAddr, superblock.inodeBitmap,
                    superblock.blockBPAddr);
            init_inode_helper(INODE_START, INODE_MAX / INODE_PER_BLOCK,
                    superblock.blockBPAddr);
            init_root_dir_helper(superblock.rootDir, superblock.blockBPAddr);
            printf("file system initialize successfully!\n");
        } else {
            printf("file system already exist.\n");
        }
    }
}

/* start address of the first block to store bitmap
 for each block is an int array */
void init_bitmap_helper(int startAddr, int len, int blockBpm) {
    int i;
    for (i = startAddr; i < (startAddr + len); i++) {
        struct Bitmap bitmap;
        memset(bitmap.bitmap, 0, BLOCKSIZE);
        write_block(i, (char *) &bitmap);
        set_bitmap_helper(i, blockBpm);
    }
}

/* start_addr: inode_start_addr
 block_bpm: the block address of the free block management */
void init_inode_helper(int startAddr, int len, int blockBpm) {
    int i;
    for (i = startAddr; i < (startAddr + len); i++) {
        struct InodeArray inode_arr;
        memset(inode_arr.inodes, 0, BLOCKSIZE);
        write_block(i, (char *) &inode_arr);
        set_bitmap_helper(i, blockBpm);
    }
}

void init_root_dir_helper(int addr, int bitmapAddr) {
    struct Dir rootDirectory;
    init_dir_helper(&rootDirectory, 1);
    write_block(addr, (char *) &rootDirectory);
    set_bitmap_helper(addr, bitmapAddr);
    set_bitmap_helper(0, 1);
}

void init_dir_helper(struct Dir *dirTmp, int iNode) {
    strcpy(dirTmp->current.filename, dot);
    strcpy(dirTmp->parent.filename, dotdot);
    dirTmp->current.inodeNum = iNode;
    dirTmp->parent.inodeNum = iNode;
    dirTmp->existNum = 0;
    int i;
    for (i = 0; i < 13; i++) {
        dirTmp->files[i].inodeNum = 0;
        memset(dirTmp->files[i].filename, 0, 60);
    }
}

/* bit_num: bit number, for example BITMAP_OFFSET is 200.
 block_addr: the block address number 2.
 Meaning that the bit-th block has been used.*/
void set_bitmap_helper(int bitNum, int blockAddr) {
    int addr = bitNum / (BITMAP_SIZE) + blockAddr;
    int bit = bitNum % (BITMAP_SIZE);
    char buff[BLOCKSIZE];
    if (read_block(addr, buff) == 0) {
        struct Bitmap *tmp = (struct Bitmap *)buff;
        seq = bit / INT_SIZE / 8;
        mask = bit % (INT_SIZE * 8);
        tmp->bitmap[seq] |= 1 << (mask);
        write_block(addr, buff);
    } else {
        printf("read block error.\n");
    }
}

/* return 1: bit has been set.
 return 0: bit hasn't been set.
 return -1: read_block error. */
int test_bitmap_helper(int bitNum, int blockAddr) {
    int addr = bitNum / BITMAP_SIZE + blockAddr;
    int bit = bitNum % BITMAP_SIZE;
    char buff[BLOCKSIZE];
    if (read_block(addr, buff) == 0) {
        struct Bitmap *tmp = (struct Bitmap *)buff;
        seq = bit / INT_SIZE / 8;
        mask = bit % (INT_SIZE * 8);
        if ((tmp->bitmap[seq] & (1 << (mask))) != 0)
            return 1;
        else
            return 0;
    }
    return -1;
}

/* Meaning that the bit-th block is to free. */
void clear_bitmap_helper(int bitNum, int blockAddr) {
    int addr = bitNum / BITMAP_SIZE + blockAddr;
    int bit = bitNum % BITMAP_SIZE;
    char buff[BLOCKSIZE];
    if (read_block(addr, buff) == 0) {
        struct Bitmap *tmp = (struct Bitmap *)buff;
        seq = bit / INT_SIZE / 8;
        mask = bit % (INT_SIZE * 8);
        tmp->bitmap[seq] &= ~(1 << (mask));
        write_block(addr, buff);
    } else {
        printf("read block error.\n");
    }
}

/* split path by '/' and return path level
 store each sub_path name in sub_path[10][60] */
int parse_path_helper(char *path, char subPath[10][60]) {
    int i, j;
    int pathLevel = 0;
    char tmp[60] = "";
    for (i = 0; i < strlen(path); i++) {
        if (path[i] == '/') {
            if (i) {
                strcpy(subPath[pathLevel], tmp);
                pathLevel++;
            }
            j = 0;
            memset(tmp, 0, sizeof(tmp));
        } else {
            tmp[j++] = path[i];
        }
    }
    strcpy(subPath[pathLevel], tmp);
    pathLevel++;
    return pathLevel;
}

/* return the free inode/block number
 block_addr is 2(free block) or 1(free inode) */
int find_free_helper(int beginner, int blockAddr) {
    int result = beginner;
    while (1) {
        result++;
        if (test_bitmap_helper(result, blockAddr) == 0) {
            return result;
        }
    }
}

/* return the next directory's block number in disc
 if isn't exist, then return -1.
 block_number is the current block number in disc */
int dir_block_find_helper(char pathName[60], int blockNumber) {
    char buff[BLOCKSIZE];
    read_block(blockNumber, buff);
    struct Dir *tmp = (struct Dir *)buff;

    int i;
    for (i = 0; i < 13; i++) {
        if (strcmp(tmp->files[i].filename, pathName) == 0) {
            return inode_to_block_helper(tmp->files[i].inodeNum);
        }
    }
    return -1;
}

/* return the next directory's inode
 if isn't exist, then return -1.
 block_number is the current block number in disc */
int dir_inode_find_helper(char pathName[60], int blockNumber) {
    int i;
    char buff[BLOCKSIZE];
    read_block(blockNumber, buff);
    struct Dir *tmp = (struct Dir *)buff;
    for (i = 0; i < 13; i++) {
        if (strcmp(tmp->files[i].filename, pathName) == 0) {
            return tmp->files[i].inodeNum;
        }
    }
    return -1;
}

/* return the block number in disc from inode
 only used for searching directory. */
int inode_to_block_helper(int inodeNumber) {
    int inodeAddr = inodeNumber / INODE_PER_BLOCK + INODE_START;
    int inodeOffset = inodeNumber % INODE_PER_BLOCK;
    char buff[BLOCKSIZE];
    read_block(inodeAddr, buff);
    struct InodeArray *tmp = (struct InodeArray *)buff;
    return tmp->inodes[inodeOffset].addr[0];
}

/* used for making directory */
void creat_inode_helper(int inodeNumber, int blockNumber) {
    int inodeAddr = inodeNumber / INODE_PER_BLOCK + INODE_START;
    int inodeOffset = inodeNumber % INODE_PER_BLOCK;
    char buff[BLOCKSIZE];
    read_block(inodeAddr, buff);
    struct InodeArray *tmp = (struct InodeArray *)buff;
    tmp->inodes[inodeOffset].addr[0] = blockNumber;
    write_block(inodeAddr, buff);
}

/* par_dir_block: the parent directory's block number
 Used to create a new directory. */
void add_dir_helper(char pathName[60], int parrentDirBlock) {
    char buff[BLOCKSIZE];
    read_block(0, buff);
    struct SuperBlock *readSuperBlock = (struct SuperBlock *)buff;
    // test whether there is still space to add a new directory
    if (readSuperBlock->blockFreeSize < 1) {
        printf("There is no space in disc.\n");
        return;
    }

    int i = readSuperBlock->inodeFree;
    int b = readSuperBlock->blockFree;
    int i_add = readSuperBlock->inodeBPAddr;
    int b_add = readSuperBlock->blockBPAddr;

    // 1. create a new directory
    struct Dir dirTmp;
    init_dir_helper(&dirTmp, i);
    write_block(b, (char *) &dirTmp);
    set_bitmap_helper(i, i_add);
    set_bitmap_helper(b, b_add);

    // 2. create a corresponding i-node
    creat_inode_helper(i, b);

    // 3. change free i_node
    int newInode = find_free_helper(i, i_add);
    readSuperBlock->inodeFree = newInode;

    // 4. change free block
    int newBlock = find_free_helper(b, b_add);
    readSuperBlock->blockFree = newBlock;

    // 5. modify superblock
    readSuperBlock->blockFreeSize--;
    write_block(0, buff);

    // 6. add current directory to its parent's directory
    add_parent_dir_helper(i, pathName, parrentDirBlock);
}

/* insert one sub directory/file record in parent's directory
 and add the exist_num */
void add_parent_dir_helper(int addInode, char pathName[60], int parentDirBlock) {
    char buffParent[BLOCKSIZE];
    read_block(parentDirBlock, buffParent);
    struct Dir *parentDir = (struct Dir *)buffParent;

    if (parentDir->existNum >= 13) {
        printf("Current directory couldn't hold more sub directorys/files.\n");
        return;
    }

    int i;
    for (i = 0; i < 13; i++) {
        if (parentDir->files[i].inodeNum == 0) {
            parentDir->files[i].inodeNum = addInode;
            strcpy(parentDir->files[i].filename, pathName);
            break;
        }
    }
    parentDir->existNum++;
    write_block(parentDirBlock, buffParent);
}

/* par_dir_block: the parent directory's block number
 cur_dir_block: the current directory's block number
 Used to remove a directory. */
void rm_dir_helper(char pathName[60], int parentDirBlock, int currentDirBlock) {
    char buff[BLOCKSIZE];
    read_block(0, buff);
    struct SuperBlock *readSuperBlock = (struct SuperBlock *)buff;

    int i = readSuperBlock->inodeFree;
    int b = readSuperBlock->blockFree;
    int i_add = readSuperBlock->inodeBPAddr;
    int b_add = readSuperBlock->blockBPAddr;

    // 1. remove the directory both in disc and its i-node bitmap
    // and there is no need to modify corresponding blocks in disc
    int currentInode = dir_inode_find_helper(pathName, parentDirBlock);
    clear_bitmap_helper(currentInode, i_add);
    clear_bitmap_helper(currentDirBlock, b_add);

    // 2. change free i_node
    if (currentInode < i)
        readSuperBlock->inodeFree = currentInode;

    // 3. change free block
    if (currentDirBlock < b)
        readSuperBlock->blockFree = currentDirBlock;

    // 4. modify superblock
    readSuperBlock->blockFreeSize++;
    write_block(0, buff);

    // 5. remove current directory from its parent's directory
    clear_parent_dir_helper(currentInode, parentDirBlock);
}

void rm_file_helper(int curentInode, int parentDirBlock) {
    char buff[BLOCKSIZE];
    read_block(0, buff);
    struct SuperBlock *readSuperBlock = (struct SuperBlock *)buff;

    int i = readSuperBlock->inodeFree;
    int b = readSuperBlock->blockFree;
    int i_add = readSuperBlock->inodeBPAddr;
    int b_add = readSuperBlock->blockBPAddr;

    int inodeAddr = curentInode / INODE_PER_BLOCK + INODE_START;
    int inodeOffset = curentInode % INODE_PER_BLOCK;
    char buffInode[BLOCKSIZE];
    read_block(inodeAddr, buffInode);
    struct InodeArray *tmp = (struct InodeArray *)buffInode;

    // change free i_node
    if (curentInode < i)
        readSuperBlock->inodeFree = curentInode;
    // clear inode bitmap
    clear_bitmap_helper(curentInode, i_add);

    // change free block
    int currentDirBlock = tmp->inodes[inodeOffset].addr[0];
    if (currentDirBlock < b)
        readSuperBlock->blockFree = currentDirBlock;

    // clear free block bitmap
    read_all_block_helper(curentInode);

    int j;
    int sum = 0;
    for (j = 0; j < INODE_ARRAR_SIZE; j++) {
        if (inodeArray[j]) {
            sum++;
            clear_bitmap_helper(inodeArray[j], b_add);
        } else {
            break;
        }
    }
    readSuperBlock->blockFreeSize += sum;
    memset(inodeArray, 0, sizeof(inodeArray));

    save_all_blocks_helper(curentInode);

    // modify superblock
    write_block(0, buff);

    // remove current file from its parent's directory
    clear_parent_dir_helper(curentInode, parentDirBlock);
}

void clear_parent_dir_helper(int deleteInode, int parentDirBlock) {
    char bufferParent[BLOCKSIZE];
    read_block(parentDirBlock, bufferParent);
    struct Dir *parentDir = ( struct Dir *)bufferParent;

    int i;
    for (i = 0; i < 13; i++) {
        if (parentDir->files[i].inodeNum == deleteInode) {
            parentDir->files[i].inodeNum = 0;
            memset(parentDir->files[i].filename, 0, 60);
            break;
        }
    }
    parentDir->existNum--;
    write_block(parentDirBlock, bufferParent);
}

int add_file_helper(char pathName[60], int parentDirBlock) {
    char buff[BLOCKSIZE];
    read_block(0, buff);
    struct SuperBlock *readSuperBlock = (struct SuperBlock *)buff;
    if (readSuperBlock->blockFreeSize < 1) {
        printf("There is no space in disc.\n");
        return -1;
    }

    int i = readSuperBlock->inodeFree;
    int b = readSuperBlock->blockFree;
    int iAdd = readSuperBlock->inodeBPAddr;
    int bAdd = readSuperBlock->blockBPAddr;

    // 1. create a new file
    set_bitmap_helper(i, iAdd);
    set_bitmap_helper(b, bAdd);

    // 2. create a corresponding i-node
    creat_inode_helper(i, b);

    // 3. change free i_node
    int newInode = find_free_helper(i, iAdd);
    readSuperBlock->inodeFree = newInode;

    // 4. change free block
    int newBlock = find_free_helper(b, bAdd);
    readSuperBlock->blockFree = newBlock;

    // 5. modify superblock
    readSuperBlock->blockFreeSize--;
    write_block(0, buff);

    // 6. add new file to its parent's directory
    add_parent_dir_helper(i, pathName, parentDirBlock);

    return i;
}

/* read and put all block numbers in disc to inode_array */
void read_all_block_helper(int inode) {
    int inodeAddr = inode / INODE_PER_BLOCK + INODE_START;
    int inodeOffset = inode % INODE_PER_BLOCK;
    char buffInode[BLOCKSIZE];
    read_block(inodeAddr, buffInode);
    struct InodeArray *tmp = (struct InodeArray *)buffInode;

    int i, j;
    int num = 0;
    char firstBuff[BLOCKSIZE];
    char secBuff[BLOCKSIZE];
    int firstNumber = tmp->inodes[inodeOffset].firstLevel;
    int secondNumber = tmp->inodes[inodeOffset].secondLevel;
    struct FirstBlock *firstBlock;
    struct SecondBlock1 *secondBlock1;
    struct SecondBlock2 *secondBlock2;

    memset(inodeArray, 0, sizeof(inodeArray));
    for (i = 0; i < 6; i++) {
        inodeArray[num++] = tmp->inodes[inodeOffset].addr[i];
    }

    if (firstNumber) {
        inodeArray[num++] = firstNumber;
        read_block(firstNumber, firstBuff);
        firstBlock = (struct FirstBlock *)firstBuff;
        for (i = 0; i < 256; i++) {
            inodeArray[num++] = firstBlock->addr[i];
        }
    } else
        return;

    if (secondNumber) {
        inodeArray[num++] = secondNumber;
        read_block(secondNumber, firstBuff);
        secondBlock1 = (struct SecondBlock1 *)firstBuff;
        for (i = 0; i < 256; i++) {
            if (secondBlock1->addr[i]) {
                inodeArray[num++] = secondBlock1->addr[i];
                read_block(secondBlock1->addr[i], secBuff);
                secondBlock2 = (struct SecondBlock2 *)secBuff;
                for (j = 0; j < 256; j++) {
                    inodeArray[num++] = secondBlock2->addr[j];
                }
            } else {
                return;
            }
        }
    }
}

/* write back all block numbers in disc from inode_array */
void save_all_blocks_helper(int inode) {
    int inodeAddr = inode / INODE_PER_BLOCK + INODE_START;
    int inodeOffset = inode % INODE_PER_BLOCK;
    char buffInode[BLOCKSIZE];
    read_block(inodeAddr, buffInode);
    struct InodeArray *tmp = (struct InodeArray *)buffInode;

    int i, j;
    int num = 0;
    int firstNumber;
    int secondNumber;
    char firstBuff[BLOCKSIZE];
    char seccondBuff[BLOCKSIZE];
    struct FirstBlock *firstBlock;
    struct SecondBlock1 *secondBlock1;
    struct SecondBlock2 *secondBlock2;
    int blockToWrite;
    for (i = 0; i < 6; i++) {
        tmp->inodes[inodeOffset].addr[i] = inodeArray[num++];
    }

    firstNumber = inodeArray[num++];
    if (firstNumber) {
        tmp->inodes[inodeOffset].firstLevel = firstNumber;
        read_block(firstNumber, firstBuff);
        firstBlock = (struct FirstBlock *)firstBuff;
        for (i = 0; i < 256; i++) {
            firstBlock->addr[i] = inodeArray[num++];
        }
        write_block(firstNumber, firstBuff);
    }

    secondNumber = inodeArray[num++];
    if (secondNumber) {
        tmp->inodes[inodeOffset].secondLevel = secondNumber;
        read_block(secondNumber, firstBuff);
        secondBlock1 = (struct SecondBlock1 *)firstBuff;

        for (i = 0; i < 256; i++) {
            blockToWrite = inodeArray[num++];
            secondBlock1->addr[i] = blockToWrite;
            if (blockToWrite) {
                read_block(blockToWrite, seccondBuff);
                secondBlock2 = (struct SecondBlock2 *)seccondBuff;
                for (j = 0; j < 256; j++) {
                    secondBlock2->addr[j] = inodeArray[num++];
                }
                write_block(blockToWrite, seccondBuff);
            } else {
                break;
            }
        }
        write_block(secondNumber, firstBuff);
    }
    write_block(inodeAddr, buffInode);
}
