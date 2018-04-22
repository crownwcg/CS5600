/*
 * file:        homework.c
 * description: skeleton file for CS 5600/7600 file system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2016
 */

#define FUSE_USE_VERSION 27
#define _GNU_SOURCE

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "fsx600.h"
#include "blkdev.h"

extern int homework_part;       /* set by '-part n' command-line option */

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 1024-byte blocks
 */
extern struct blkdev *disk;

/* by defining bitmaps as 'fd_set' pointers, you can use existing
 * macros to handle them. 
 *   FD_ISSET(##, inode_map);
 *   FD_CLR(##, block_map);
 *   FD_SET(##, block_map);
 */
fd_set *inode_map;              /* = malloc(sb.inode_map_size * FS_BLOCK_SIZE); */
fd_set *block_map;

fd_set *inode_map;
int     inode_map_base;

struct fs_inode *inodes;
int   n_inodes;
int   inode_base;

fd_set *block_map;
int     block_map_base;

int n_blocks;
int root_inode;

// constants for read/write dir entries 
int N_ENTRIES = FS_BLOCK_SIZE / sizeof(struct fs_dirent);

// constants for read/write
int NUM_PER_BLK = BLOCK_SIZE / sizeof(uint32_t);
int DIRECT_SIZE = N_DIRECT * BLOCK_SIZE;
int INDIR1_SIZE = BLOCK_SIZE / sizeof(uint32_t) * BLOCK_SIZE;
int INDIR2_SIZE = BLOCK_SIZE / sizeof(uint32_t) * BLOCK_SIZE / sizeof(uint32_t) * BLOCK_SIZE;

/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(struct fuse_conn_info *conn)
{
    struct fs_super sb;
    if (disk->ops->read(disk, 0, 1, &sb) < 0)
        exit(1);

    /* The inode map and block map are written directly to the disk after the superblock */

    inode_map_base = 1;
    inode_map = malloc(sb.inode_map_sz * FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inode_map_base, sb.inode_map_sz, inode_map) < 0)
        exit(1);

    block_map_base = inode_map_base + sb.inode_map_sz;
    block_map = malloc(sb.block_map_sz * FS_BLOCK_SIZE);
    if (disk->ops->read(disk, block_map_base, sb.block_map_sz, block_map) < 0)
        exit(1);

    /* The inode data is written to the next set of blocks */

    inode_base = block_map_base + sb.block_map_sz;
    n_inodes = sb.inode_region_sz * INODES_PER_BLK;
    inodes = malloc(sb.inode_region_sz * FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inode_base, sb.inode_region_sz, inodes) < 0)
        exit(1);

    /* your code here */
    n_blocks   = sb.num_blocks;
    root_inode = sb.root_inode;

    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */
/* get the inode index of the path
 */
static int lookup(const char *path) {
    if (strcmp(path, "/") == 0) 
        return root_inode;
    // copy the path
    char _path[strlen(path) + 1];
    strcpy(_path, path);

    // parent inode and directory pointer
    int inode_index = root_inode;
    struct fs_inode  *curr_dir_inode = NULL;
    struct fs_dirent *curr_dir = NULL;
    struct fs_dirent  entries[N_ENTRIES];

    // traverse the path  
    char *token = strtok(_path, "/");
    while (token != NULL && strlen(token) > 0) {
        if (curr_dir != NULL) {
            if (!curr_dir->valid)
                return -EINVAL;
            if (!curr_dir->isDir) 
                return -ENOTDIR;
        }

        curr_dir_inode = &inodes[inode_index];
        bzero(entries, N_ENTRIES * sizeof(struct fs_dirent));
        if (disk->ops->read(disk, curr_dir_inode->direct[0], 1, entries) < 0)
            exit(1);

        // look up for the dir name
        int i;
        for (i = 0; i < N_ENTRIES; i++) {
            if (entries[i].valid && strcmp(entries[i].name, token) == 0) {
                inode_index = entries[i].inode;
                curr_dir = &entries[i];
                break;
            }
        }

        // if not found
        if (i == N_ENTRIES) 
            return -ENOENT;

        token = strtok(NULL, "/");
    }

    return inode_index;
}

/* set attributes of the stat structure
 */
static void setStat(struct fs_inode *inode, struct stat *sb) {
    memset(sb, 0, sizeof(*sb));
    sb->st_uid    = inode->uid;
    sb->st_gid    = inode->gid;
    sb->st_mode   = inode->mode;
    sb->st_atime  = inode->mtime;
    sb->st_ctime  = inode->ctime;
    sb->st_mtime  = inode->mtime;
    sb->st_size   = inode->size;
    sb->st_nlink  = 1;
    sb->st_blocks = (inode->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
}

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in fsx600 are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int fs_getattr(const char *path, struct stat *sb)
{   
    // init file system
    fs_init(NULL);

    // look up for inode of the path
    int inode_index = lookup(path);
    if (inode_index < 0) 
        return inode_index;
    setStat(&inodes[inode_index], sb);
    return SUCCESS;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{   
    // check the path is a directory
    char _path[strlen(path) + 1]; 
    strcpy(_path, path);

    // return an error if not successfully looked up
    int inode_index = lookup(_path);
    if (inode_index < 0)
        return inode_index;
    // return ENOTDIR if not a dir
    struct fs_inode *inode = &inodes[inode_index];
    if (!S_ISDIR(inode->mode))
        return -ENOTDIR;

    // get directory information
    struct fs_dirent entries[N_ENTRIES];
    struct stat      sb;
    if (disk->ops->read(disk, inode->direct[0], 1, entries) < 0)
        exit(1);
    for (int i = 0; i < N_ENTRIES; i++) {
        if (entries[i].valid) {
            setStat(&inodes[entries[i].inode], &sb);
            filler(ptr, entries[i].name, &sb, 0);
        }
    }

    return SUCCESS;
}

/* check the path is a directory
 * error:
 * ENOENT if dir doesn't exist
 * ENOTDIR if path is not dir
 */
static int is_dir(const char *path) {
    char _path[strlen(path) + 1]; 
    strcpy(_path, path);

    // return an error if not successfully looked up
    int inode_index  = lookup(_path);
    if (inode_index < 0)
        return -ENOENT;

    // return ENOTDIR if not a dir
    if (!S_ISDIR(inodes[inode_index].mode))
        return -ENOTDIR;

    return SUCCESS;
}

/* see description of Part 2. In particular, you can save information 
 * in fi->fh. If you allocate memory, free it in fs_releasedir.
 */
static int fs_opendir(const char *path, struct fuse_file_info *fi)
{
    int val;
    if ((val = is_dir(path)) == SUCCESS)
        fi->fh = lookup(path);

    return val;
}

static int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int val;
    if ((val = is_dir(path)) == SUCCESS)
        fi->fh = -1;

    return val;
}

/* find free inode index of the map
 */
static int find_free_inode_index() {
    for (int i = 2; i < n_inodes; i++) 
        if (!FD_ISSET(i, inode_map)) 
            return i;

    return -ENOSPC;
}

/* find free block index of the disk
 */
static int find_free_blk_num() {
    for (int i = 0; i < n_blocks; i++) {
        if (!FD_ISSET(i, block_map)) {
            char clear_buffer[BLOCK_SIZE];
            bzero(clear_buffer, BLOCK_SIZE);
            if (disk->ops->write(disk, i, 1, clear_buffer) < 0)
                exit(1);
            return i;
        }
    }

    return -ENOSPC;
}

/* update inode map
 */
static void update_map() {
    if (disk->ops->write(disk, 
                         inode_map_base, 
                         block_map_base - inode_map_base, 
                         inode_map) < 0)
        exit(1);
    if (disk->ops->write(disk, 
                         block_map_base, 
                         inode_base - block_map_base, 
                         block_map) < 0)
        exit(1);
}

/* write inode update to the disk
 */
static void update_inode(int inode_index) {
    if (disk->ops->write(disk, 
        inode_base + inode_index / INODES_PER_BLK,
        1,
        &inodes[inode_index - (inode_index % INODES_PER_BLK)]) < 0)
        exit(1);
}

/* mknod - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *          in particular, for mknod("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If this would result in >32 entries in a directory, return -ENOSPC
 * if !S_ISREG(mode) return -EINVAL [i.e. 'mode' specifies a device special
 * file or other non-file object]
 */
static int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
    // make sure mode is regular file
    mode |= S_IFREG;
    if (!S_ISREG(mode) || strcmp(path, "/") == 0)
        return -EINVAL;

    // get parent dir
    char parent[strlen(path) + 1];
    strcpy(parent, path);
    parent[strrchr(parent, '/') - parent] = '\0';
    if (strlen(parent) == 0)
        strcpy(parent, "/");

    // check parent dir inode index exists
    int dir_inode_index = lookup(parent);
    if (dir_inode_index < 0) 
        return dir_inode_index;

    // check whether the file already exists
    int inode_index = lookup(path);
    if (inode_index >= 0) 
        return -EEXIST;

    // find parent dir inode
    struct fs_inode *dir_inode = &inodes[dir_inode_index];
    if (!S_ISDIR(dir_inode->mode)) 
        return -ENOTDIR;
    struct fs_dirent entries[N_ENTRIES];
    if (disk->ops->read(disk, dir_inode->direct[0], 1, entries) < 0)
        exit(1);

    // find free dir entry
    int free_dir_entry;
    for (free_dir_entry = 0; free_dir_entry < N_ENTRIES; free_dir_entry++) {
        if (!entries[free_dir_entry].valid) {
            break;
        }
    }
    if (free_dir_entry == N_ENTRIES) 
        return -ENOSPC;

    // create file
    // copy file name
    char *file_name = strrchr(path, '/') + 1;
    if (strlen(file_name) >= 28) 
        return -EINVAL;
    strcpy(entries[free_dir_entry].name, file_name);

    // allocate inode, find free inode
    int free_inode_index = find_free_inode_index();
    if (free_inode_index < 0) 
        return -ENOSPC;
    entries[free_dir_entry].inode = free_inode_index;
    entries[free_dir_entry].isDir = false;
    entries[free_dir_entry].valid = true;
    FD_SET(free_inode_index, inode_map);

    // set attributes of inode
    time_t cmtime = time(NULL);
    inodes[free_inode_index].uid   = getuid();
    inodes[free_inode_index].gid   = getgid();
    inodes[free_inode_index].mode  = mode;
    inodes[free_inode_index].ctime = cmtime;
    inodes[free_inode_index].mtime = cmtime;
    inodes[free_inode_index].size  = 0;

    // update and write to disk
    update_inode(free_inode_index);
    update_map();
    if (disk->ops->write(disk, dir_inode->direct[0], 1, entries) < 0)
        exit(1);

    return SUCCESS;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create. 
 * If this would result in >32 entries in a directory, return -ENOSPC
 *
 * Note that you may want to combine the logic of fs_mknod and
 * fs_mkdir. 
 */ 
static int fs_mkdir(const char *path, mode_t mode)
{   
    // make sure mode is directory
    mode |= S_IFDIR;
    if (!S_ISDIR(mode) || strcmp(path, "/") == 0)
        return -EINVAL;

    char _path[strlen(path)];
    strcpy(_path, path);
    // get parent dir
    char parent[strlen(_path) + 1];
    strcpy(parent, _path);
    parent[strrchr(parent, '/') - parent] = '\0';
    if (strlen(parent) == 0) 
        strcpy(parent, "/");

    // get parent dir inode index
    int dir_inode_index = lookup(parent);
    if (dir_inode_index < 0) 
        return dir_inode_index;

    // check the whether the directory name exists
    int inode_index = lookup(_path);
    if (inode_index >= 0) 
        return -EEXIST;

    // get parent dir inode
    struct fs_inode *dir_inode = &inodes[dir_inode_index];
    if (!S_ISDIR(dir_inode->mode)) 
        return -ENOTDIR;

    // find free dir entry
    struct fs_dirent entries[N_ENTRIES];
    if (disk->ops->read(disk, dir_inode->direct[0], 1, entries) < 0)
        exit(1);
    int free_dir_entry;
    for (free_dir_entry = 0; free_dir_entry < N_ENTRIES; free_dir_entry++) {
        if (!entries[free_dir_entry].valid) {
            break;
        }
    }
    if (free_dir_entry == N_ENTRIES) 
        return -ENOSPC;

    // initiate entry
    // copy dir name
    char *dir_name = strrchr(_path, '/') + 1;
    if (strlen(dir_name) >= 28)
        return -EINVAL;
    strcpy(entries[free_dir_entry].name, dir_name);
    // allocate inode
    int free_inode_index = find_free_inode_index();
    if (free_inode_index < 0) 
        return -ENOSPC;
    entries[free_dir_entry].inode = free_inode_index;
    entries[free_dir_entry].isDir = true;
    entries[free_dir_entry].valid = true;
    FD_SET(free_inode_index, inode_map);

    // set attributes of inode
    time_t cmtime = time(NULL);
    inodes[free_inode_index].uid   = getuid();
    inodes[free_inode_index].gid   = getgid();
    inodes[free_inode_index].mode  = mode;
    inodes[free_inode_index].ctime = cmtime;
    inodes[free_inode_index].mtime = cmtime;
    inodes[free_inode_index].size  = 0;

    // allocate block
    int free_blk_num = find_free_blk_num();
    if (free_blk_num < 0) 
        return -ENOSPC;
    FD_SET(free_blk_num, block_map);
    inodes[free_inode_index].direct[0] = free_blk_num;

    // update and write to disk
    update_inode(free_inode_index);
    update_map();
    if (disk->ops->write(disk, dir_inode->direct[0], 1, entries) < 0)
        exit(1);

    return SUCCESS;
}

/* clear the indir1 blocks of file
 */
static void truncate_clear_indir1(int blk_num) {
    // read from blocks
    uint32_t buffer[NUM_PER_BLK];
    bzero(buffer, BLOCK_SIZE);
    if (disk->ops->read(disk, blk_num, 1, buffer) < 0)
        exit(1);

    // clear the blocks
    for (int i = 0; i < NUM_PER_BLK; i++) {
        if (buffer[i])
            FD_CLR(buffer[i], block_map);
    }
    FD_CLR(blk_num, block_map);
}

/* clear the indir2 blocks of file
 */
static void truncate_clear_indir2(int blk_num) {
    // read from blocks
    uint32_t buffer[NUM_PER_BLK];
    bzero(buffer, BLOCK_SIZE);
    if (disk->ops->read(disk, blk_num, 1, buffer) < 0)
        exit(1);

    // clear the blocks
    for (int i = 0; i < NUM_PER_BLK; i++) {
        if (buffer[i])
            truncate_clear_indir1(buffer[i]);
    }
    FD_CLR(blk_num, block_map);
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int fs_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
	   return -EINVAL;		/* invalid argument */

    // get inode
    int inode_index = lookup(path);
    if (inode_index < 0) 
        return inode_index;
    struct fs_inode *inode = &inodes[inode_index];
    if (S_ISDIR(inode->mode))
        return -EISDIR;

    // clear the block of the inode
    for (int i = 0; i < N_DIRECT; i++) {
        if (inode->direct[i]) 
            FD_CLR(inode->direct[i], block_map);
        inode->direct[i] = 0;
    }

    // clear indir 1 blocks if it exists
    if (inode->indir_1) 
        truncate_clear_indir1(inode->indir_1);

    // clear indir 2 blocks if it exists
    if (inode->indir_2) {
        truncate_clear_indir2(inode->indir_2);
    }

    // set size
    inode->size    = 0;
    inode->indir_1 = 0;
    inode->indir_2 = 0;
    update_inode(inode_index);
                update_map();

    return SUCCESS;
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 * Note that you have to delete (i.e. truncate) all the data.
 */
static int fs_unlink(const char *path)
{   
    // truncate all data
    int status;
    if ((status = fs_truncate(path, 0)) != SUCCESS)
        return status;

    // get parent dir
    char parent[strlen(path) + 1];
    strcpy(parent, path);
    parent[strrchr(parent, '/') - parent] = '\0';
    if (strlen(parent) == 0)
        strcpy(parent, "/");
    struct fs_inode *parent_inode = &inodes[lookup(parent)];

    // get file name
    char *file_name = strrchr(path, '/') + 1;;

    // remove entry
    struct fs_dirent entries[N_ENTRIES];
    if (disk->ops->read(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    for (int i = 0; i < N_ENTRIES; i++) 
        if (entries[i].valid && strcmp(entries[i].name, file_name) == 0) 
            memset(&entries[i], 0, sizeof(struct fs_dirent));

    // set inode
    int inode_index = lookup(path);
    struct fs_inode *inode = &inodes[inode_index];
    memset(inode, 0, sizeof(struct fs_inode));
    FD_CLR(inode_index, inode_map);
    update_inode(inode_index);
    update_map();
    if (disk->ops->write(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);

    return SUCCESS;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int fs_rmdir(const char *path)
{   
    // in case remove root dir
    if (strcmp(path, "/") == 0)
        return -EINVAL;

    // get parent dir
    char parent[strlen(path) + 1];
    strcpy(parent, path);
    parent[strrchr(parent, '/') - parent] = '\0';
    if (strlen(parent) == 0)
        strcpy(parent, "/");

    // get parent dir inode index
    int dir_inode_index = lookup(parent);
    if (dir_inode_index < 0)
        return dir_inode_index;
    struct fs_inode *dir_inode = &inodes[dir_inode_index];
    if (!S_ISDIR(dir_inode->mode))
        return -ENOTDIR;

    // get dir inode index
    int inode_index = lookup(path);
    if (inode_index < 0)
        return inode_index;
    struct fs_inode *inode = &inodes[inode_index];
    if (!S_ISDIR(dir_inode->mode))
        return -ENOTDIR;

    // check dir is empty
    struct fs_dirent entries[N_ENTRIES];
    if (disk->ops->read(disk, inode->direct[0], 1, entries) < 0)
        exit(1);
    for (int i = 0; i < N_ENTRIES; i++) 
        if (entries[i].valid)
            return -ENOTEMPTY;

    // get dir name
    char *dir_name = strrchr(path, '/') + 1;

    // read parent dir entries
    struct fs_dirent dir_entries[N_ENTRIES];
    if (disk->ops->read(disk, dir_inode->direct[0], 1, dir_entries) < 0)
        exit(1);

    // unlink: clear dir entries and inode 
    for (int i = 0; i < N_ENTRIES; i++) 
        if (dir_entries[i].valid && strcmp(dir_entries[i].name, dir_name) == 0) 
            memset(&dir_entries[i], 0, sizeof(dir_entries[i]));
    memset(inode, 0, sizeof(inode));

    // remove dir and inode from map
    FD_CLR(inode->direct[0], block_map);
    FD_CLR(inode_index, inode_map);
    update_inode(inode_index);
    update_map();
    if (disk->ops->write(disk, dir_inode->direct[0], 1, dir_entries) < 0)
        exit(1);

    return SUCCESS;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int fs_rename(const char *src_path, const char *dst_path)
{       
    // check existence of source and destination
    int src_inode_index = lookup(src_path);
    if (src_inode_index < 0) 
        return src_inode_index;
    int dst_inode_index = lookup(dst_path);
    if (dst_inode_index >= 0)
        return -EEXIST;

    // get parent dir
    char src_parent[strlen(src_path) + 1];
    char dst_parent[strlen(dst_path) + 1];
    strcpy(src_parent, src_path);
    strcpy(dst_parent, dst_path);
    src_parent[strrchr(src_parent, '/') - src_parent] = '\0';
    dst_parent[strrchr(dst_parent, '/') - dst_parent] = '\0';
    if (strcmp(src_parent, dst_parent) != 0)
        return -EINVAL;

    // get dir inode
    int dir_inode_index = lookup(src_parent);
    if (dir_inode_index < 0) 
        return dir_inode_index;
    struct fs_inode *dir_inode = &inodes[dir_inode_index];

    // get name
    char *src_name = strrchr(src_path, '/') + 1;
    char *dst_name = strrchr(dst_path, '/') + 1;
    if (strlen(dst_name) >= 28)
        return -EINVAL;

    // rename
    struct fs_dirent entries[N_ENTRIES];
    if (disk->ops->read(disk, dir_inode->direct[0], 1, entries) < 0) 
        exit(1);
    for (int i = 0; i < N_ENTRIES; i++) {
        if (entries[i].valid && strcmp(entries[i].name, src_name) == 0) {
            memset(entries[i].name, 0, sizeof(entries[i].name));
            strcpy(entries[i].name, dst_name);
        }
    }
    if (disk->ops->write(disk, dir_inode->direct[0], 1, entries) < 0)
        exit(1);

    return SUCCESS;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int fs_chmod(const char *path, mode_t mode)
{
    char _path[strlen(path) + 1];
    strcpy(_path, path);
    int inode_index = lookup(_path);
    if (inode_index < 0) 
        return -ENOENT;
    struct fs_inode *inode = &inodes[inode_index];
    inode->mode = mode | (S_ISDIR(inode->mode) ? S_IFDIR : S_IFREG);
    update_inode(inode_index);
    return SUCCESS;
}

int fs_utime(const char *path, struct utimbuf *ut)
{   
    char _path[strlen(path) + 1];
    strcpy(_path, path);
    int inode_index = lookup(_path);
    if (inode_index < 0) 
        return inode_index;
    struct fs_inode *inode = &inodes[inode_index];
    inode->mtime = ut->modtime;
    update_inode(inode_index);
    return SUCCESS;
}

/* read block 
 */
static int fs_read_block(int blk_num, off_t offset, int len, char *buf) {
    char buffer[BLOCK_SIZE];
    bzero(buffer, BLOCK_SIZE);
    if (disk->ops->read(disk, blk_num, 1, buffer) < 0)
        exit(1);
    memcpy(buf, buffer + offset, len);
    return len;
}

/* read direct blocks of the inode
 */
static int fs_read_direct(struct fs_inode *inode, off_t offset, size_t len, char *buf) {
    size_t len_read, len_t = len;
    for (int blk_num = offset / BLOCK_SIZE, blk_offset = offset % BLOCK_SIZE; 
         blk_num < N_DIRECT && len_t > 0; 
         blk_num++, blk_offset = 0) {
        // calculate length to read
        len_read = blk_offset + len_t > BLOCK_SIZE ? BLOCK_SIZE - blk_offset : len_t;

        // try to read
        if (!inode->direct[blk_num]) 
            return len - len_t;
        len_read = fs_read_block(inode->direct[blk_num], blk_offset, len_read, buf);

        // increase buffer
        buf   += len_read;
        len_t -= len_read;
    }
    return len - len_t;
}

/* read indir1 blocks of the inode
 */
static int fs_read_indir1(size_t blk, off_t offset, int len, char *buf) {
    uint32_t blk_index[NUM_PER_BLK];
    if (disk->ops->read(disk, blk, 1, blk_index) < 0)
        exit(1);

    size_t len_read, len_t = len;
    for (int blk_num = offset / BLOCK_SIZE, blk_offset = offset % BLOCK_SIZE; 
         blk_num < NUM_PER_BLK && len_t > 0; 
         blk_num++, blk_offset = 0) {
        // calculate length to read
        len_read = blk_offset + len_t > BLOCK_SIZE ? BLOCK_SIZE - blk_offset : len_t;

        // try to read
        if (!blk_index[blk_num]) 
            return len - len_t;
        len_read = fs_read_block(blk_index[blk_num], blk_offset, len_read, buf);

        // increase buffer
        buf   += len_read;
        len_t -= len_read;
    }
    return len - len_t;
}

/* read indir2 blocks of the inode
 */
static int fs_read_indir2(size_t blk, off_t offset, int len, char *buf) {
    uint32_t blk_index[NUM_PER_BLK];
    if (disk->ops->read(disk, blk, 1, blk_index) < 0)
        return 0;

    size_t len_read, len_t = len;
    for (int blk_num = offset / INDIR1_SIZE, blk_offset = offset % INDIR1_SIZE; 
         blk_num < NUM_PER_BLK && len_t > 0; 
         blk_num++, blk_offset = 0) {
        // calculate length to read
        len_read = blk_offset + len_t > INDIR1_SIZE ? INDIR1_SIZE - blk_offset : len_t;

        //read 
        len_read = fs_read_indir1(blk_index[blk_num], blk_offset, len_read, buf);

        // increase buffer
        buf   += len_read;
        len_t -= len_read;
    }
    return len - len_t;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return bytes from offset to EOF
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int fs_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
    char _path[strlen(path) + 1];
    strcpy(_path, path);

    // get file inode
    int inode_index = lookup(_path);
    if (inode_index < 0)
        return inode_index;
    struct fs_inode *inode = &inodes[inode_index];
    if (!S_ISREG(inode->mode))
        return -EISDIR;

    // if offset >= file len, return 0
    if (offset >= inode->size)
        return 0;

    // if offset+len > file len, return bytes from offset to EOF
    if (offset + len > inode->size)
        len = inode->size - offset;

    size_t len_t = len;
    size_t len_read;

    // read direct blocks
    if (len_t > 0 && offset < DIRECT_SIZE) {
        len_read  = fs_read_direct(inode, offset, len_t, buf);
        offset   += len_read;
        len_t    -= len_read;
        buf      += len_read;
    }

    // read indirect 1 blocks
    if (len_t > 0 && offset < DIRECT_SIZE + INDIR1_SIZE) {
        len_read  = fs_read_indir1(inode->indir_1, offset - DIRECT_SIZE, len_t, buf);
        offset   += len_read;
        len_t    -= len_read;
        buf      += len_read;
    }

    // read indirect 2 blocks
    if (len_t > 0 && offset < DIRECT_SIZE + INDIR1_SIZE + INDIR2_SIZE) {
        len_read  = fs_read_indir2(inode->indir_2, offset - DIRECT_SIZE - INDIR1_SIZE, len_t, buf);
        offset   += len_read;
        len_t    -= len_read;
        buf      += len_read;
    }

    return len - len_t;
}

/* write direct blocks of the inode
 */
static int fs_write_direct(size_t inode_index, off_t offset, size_t len, const char *buf) {
    struct fs_inode *inode = &inodes[inode_index];
    size_t len_write, len_t = len;
    for (int blk_num = offset / BLOCK_SIZE, blk_offset = offset % BLOCK_SIZE; 
         blk_num < N_DIRECT && len_t > 0; 
         blk_num++, blk_offset = 0) {
        // calculate length to write
        len_write = blk_offset + len_t > BLOCK_SIZE ? BLOCK_SIZE - blk_offset : len_t;
        len_t    -= len_write;

        // allocate block if not exists
        if (!inode->direct[blk_num]) {
            int allocate_blk_num = find_free_blk_num();
            if (allocate_blk_num < 0)
                return len - len_t;
            inode->direct[blk_num] = allocate_blk_num;
            update_inode(inode_index);
            FD_SET(allocate_blk_num, block_map);
        }

        // try to write
        char buffer[BLOCK_SIZE];
        if (disk->ops->read(disk, inode->direct[blk_num], 1, buffer) < 0)
            exit(1);
        memcpy(buffer + blk_offset, buf, len_write);
        if (disk->ops->write(disk, inode->direct[blk_num], 1, buffer) < 0)
            exit(1);

        // increase buffer
        buf += len_write;
    }
    return len - len_t;
}

/* write indir1 blocks of the inode
 */
static int fs_write_indir1(size_t blk, off_t offset, int len, const char *buf) {
    uint32_t blk_index[NUM_PER_BLK];
    if (disk->ops->read(disk, blk, 1, blk_index) < 0)
        exit(1);

    size_t len_write, len_t = len;
    for (int blk_num = offset / BLOCK_SIZE, blk_offset = offset % BLOCK_SIZE; 
         blk_num < NUM_PER_BLK && len_t > 0; 
         blk_num++, blk_offset = 0) {
        // calculate length to read
        len_write = blk_offset + len_t > BLOCK_SIZE ? BLOCK_SIZE - blk_offset : len_t;
        len_t    -= len_write;

        // allocate block if not exists
        if (!blk_index[blk_num]) {
            int allocate_blk_num = find_free_blk_num();
            if (allocate_blk_num < 0)
                return len - len_t;
            blk_index[blk_num] = allocate_blk_num;
            // write to disk of updates of indir1 blocks
            if (disk->ops->write(disk, blk, 1, blk_index))
                exit(1);
            FD_SET(allocate_blk_num, block_map);
        }

        // try to write
        char buffer[BLOCK_SIZE];
        if (disk->ops->read(disk, blk_index[blk_num], 1, buffer) < 0)
            exit(1);
        memcpy(buffer + blk_offset, buf, len_write);
        if (disk->ops->write(disk, blk_index[blk_num], 1, buffer) < 0)
            exit(1);

        // increase buffer
        buf += len_write;
    }
    return len - len_t;
}

/* read indir2 blocks of the inode
 */
static int fs_write_indir2(size_t blk, off_t offset, int len, const char *buf) {
    uint32_t blk_index[NUM_PER_BLK];
    if (disk->ops->read(disk, blk, 1, blk_index) < 0)
        exit(1);

    size_t len_write, len_t = len;
    for (int blk_num = offset / BLOCK_SIZE, blk_offset = offset % BLOCK_SIZE; 
         blk_num < NUM_PER_BLK && len_t > 0; 
         blk_num++, blk_offset = 0) {
        // calculate length to read
        len_write = blk_offset + len_t > INDIR1_SIZE ? INDIR1_SIZE - blk_offset : len_t;
        len_t    -= len_write;

        // allocate block if not exists
        if (!blk_index[blk_num]) {
            int allocate_blk_num = find_free_blk_num();
            if (allocate_blk_num < 0)
                return len - len_t;
            blk_index[blk_num] = allocate_blk_num;
            // write to disk of updates of indir1 blocks
            if (disk->ops->write(disk, blk, 1, blk_index))
                exit(1);
            FD_SET(allocate_blk_num, block_map);
        }
        
        // try to write
        len_write = fs_write_indir1(blk_index[blk_num], blk_offset, len_write, buf);
        if (len_write == 0)
            return len - len_t;

        // increase buffer
        buf += len_write;
    }
    return len - len_t;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them, 
 *   but we don't)
 */
static int fs_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
    char _path[strlen(path) + 1];
    strcpy(_path, path);

    // get file inode
    int inode_index = lookup(_path);
    if (inode_index < 0)
        return inode_index;
    struct fs_inode *inode = &inodes[inode_index];
    if (!S_ISREG(inode->mode))
        return -EISDIR;

    // return EINVAL if 'offset' is greater than current file length.
    if (offset > inode->size)
        return 0;

    // read file
    size_t len_t = len;
    size_t len_write;

    // write direct blocks
    if (len_t > 0 && offset < DIRECT_SIZE) {
        len_write = fs_write_direct(inode_index, offset, len_t, buf);
        offset   += len_write;
        len_t    -= len_write;
        buf      += len_write;
    }

    // write indirect 1 blocks
    if (len_t > 0 && offset < DIRECT_SIZE + INDIR1_SIZE) {
        // allocate indir1
        if (!inode->indir_1) {
            int blk_num = find_free_blk_num();
            if (blk_num < 0)
                return len_t - len;
            inode->indir_1 = blk_num;
            update_inode(inode_index);
            FD_SET(blk_num, block_map);
        }

        // write to indir 1
        len_write = fs_write_indir1(inode->indir_1, offset - DIRECT_SIZE, len_t, buf);
        offset   += len_write;
        len_t    -= len_write;
        buf      += len_write;
    }

    // write indirect 2 blocks
    if (len_t > 0 && offset < DIRECT_SIZE + INDIR1_SIZE + INDIR2_SIZE) {
        // allocate indir2
        if (!inode->indir_2) {
            int blk_num = find_free_blk_num();
            if (blk_num < 0)
                return len_t - len;
            inode->indir_2 = blk_num;
            update_inode(inode_index);
            FD_SET(blk_num, block_map);
        }

        // write to indir 2
        len_write  = fs_write_indir2(inode->indir_2, offset - DIRECT_SIZE - INDIR1_SIZE, len_t, buf);
        offset   += len_write;
        len_t    -= len_write;
        buf      += len_write;
    }

    if (offset > inode->size) {
        inode->size = offset;
        update_inode(inode_index);
    }
    update_map();

    return len - len_t;
}

/* check the path is a file
 * error:
 * ENOENT if file doesn't exist
 * EISDIR if file is a directory
 */
static int is_file(const char *path) {
    char _path[strlen(path) + 1]; 
    strcpy(_path, path);

    // ENOENT if file doesn't exist
    int inode_index = lookup(_path);
    if (inode_index < 0)
        return -ENOENT;

    // EISDIR if file is a directory
    if (S_ISDIR(inodes[inode_index].mode))
        return -EISDIR;

    return SUCCESS;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{   
    int val;
    if ((val = is_file(path)) == SUCCESS)
        fi->fh = lookup(path);

    return val;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
    int val;
    if ((val = is_file(path)) == SUCCESS) 
        fi->fh = -1;

    return val;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. 
 */
static int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - metadata
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * this should work fine, but you may want to add code to
     * calculate the correct values later.
     */
    memset(st, 0, sizeof(*st));
    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = n_blocks - root_inode - inode_base;

    size_t blocks_used = 0;
    for (int i = 0; i < n_blocks; i++) 
        if (FD_ISSET(i, block_map))
            blocks_used++;
    st->f_bfree = n_blocks - blocks_used;
    st->f_bavail = st->f_bfree; 
    st->f_namemax = 27;

    return 0;
}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'fs_ops'.
 */
struct fuse_operations fs_ops = {
    .init = fs_init,
    .getattr = fs_getattr,
    .opendir = fs_opendir,
    .readdir = fs_readdir,
    .releasedir = fs_releasedir,
    .mknod = fs_mknod,
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .utime = fs_utime,
    .truncate = fs_truncate,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .release = fs_release,
    .statfs = fs_statfs,
};