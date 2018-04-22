#include "blkdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#define BLK_NUM 32
#define TESTS_NUM 10

/* Write some data to an area of memory */
void write_data(char* data, int length){
    for (int i = 0; i < length; i++){
        data[i] = (char) i;
    }
}

/* Create a new file ready to be used as an image. Every byte of the file will be zero. */
struct blkdev *create_new_image(char * path, int blocks){
    if (blocks < 1){
        printf("create_new_image: error - blocks must be at least 1: %d\n", blocks);
        return NULL;
    }
    FILE * image = fopen(path, "w");
    /* This is a trick: instead of writing every byte from 0 to N we can instead move the file cursor
     * directly to N-1 and then write 1 byte. The filesystem will fill in the rest of the bytes with
     * zero for us.
     */
    fseek(image, blocks * BLOCK_SIZE - 1, SEEK_SET);
    char c = 0;
    fwrite(&c, 1, 1, image);
    fclose(image);

    return image_create(path);
}

/* Write a buffer to a file for debugging purposes */
void dump(char* buffer, int length, char* path){
    FILE * output = fopen(path, "w");
    fwrite(buffer, 1, length, output);
    fclose(output);
}

int main() {
    struct blkdev* mirror_drives[2];
    /* Create two images for the mirror */
    mirror_drives[0] = create_new_image("mirror1", 2);
    mirror_drives[1] = create_new_image("mirror2", 2);
    /* Create the raid mirror */
    struct blkdev *mirror = mirror_create(mirror_drives);

    /* Write some data to the mirror, then read the data back and check that the
     * two buffers contain the same bytes.
     */
    char write_buffer[BLOCK_SIZE];
    write_data(write_buffer, BLOCK_SIZE);
    if (blkdev_write(mirror, 0, 1, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        exit(0);
    }

    char read_buffer[BLOCK_SIZE];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, BLOCK_SIZE);

    if (blkdev_read(mirror, 0, 1, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        exit(0);
    }

    /* For debugging, you can analyze these files manually */
    dump(write_buffer, BLOCK_SIZE, "write-buffer");
    dump(read_buffer, BLOCK_SIZE, "read-buffer");

    if (memcmp(write_buffer, read_buffer, BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
    } else {
        printf("Mirror test passed\n");
    }

    /* you code here */
    srand(time(NULL));

    blkdev_close(mirror);
    mirror_drives[0] = create_new_image("mirror1", BLK_NUM);
    mirror_drives[1] = create_new_image("mirror2", BLK_NUM);
    mirror = mirror_create(mirror_drives);
    int nblks = blkdev_num_blocks(mirror);

    // check the correct length
    assert(nblks == blkdev_num_blocks(mirror_drives[0]));

    // random write and read
    char write_buf[BLOCK_SIZE * 2];
    char read_buf[BLOCK_SIZE * 2];
    write_data(write_buf, BLOCK_SIZE);

    // random test
    char backup[BLOCK_SIZE * BLK_NUM];
    bzero(backup, BLOCK_SIZE * BLK_NUM);
    assert(blkdev_write(mirror, 0, BLK_NUM, backup) == SUCCESS);

    for (int i = 0; i < TESTS_NUM; i++) {
        int first_blk = rand() % nblks - 1;
        if (first_blk < 0) continue;
        assert(blkdev_write(mirror, first_blk, 2, write_buf) == SUCCESS);
        memcpy(backup + first_blk * BLOCK_SIZE, write_buf, BLOCK_SIZE * 2);
        assert(blkdev_read(mirror, first_blk, 2, read_buf) == SUCCESS);
        assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
        bzero(read_buf, BLOCK_SIZE);
    }

    char image_copy[BLOCK_SIZE * BLK_NUM];
    assert(blkdev_read(mirror, 0, BLK_NUM, image_copy) == SUCCESS);
    assert(memcpy(backup, image_copy, BLOCK_SIZE * BLK_NUM));

    // force one image to fail
    image_fail(mirror_drives[0]);
    assert(blkdev_write(mirror, 0, 2, write_buf) == SUCCESS);
    assert(blkdev_read(mirror, 0, 2, read_buf) == SUCCESS);
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);

    // replace failed image
    struct blkdev *newdisk = create_new_image("newdisk", BLK_NUM);
    assert(mirror_replace(mirror, 0, newdisk) == SUCCESS);

    bzero(read_buf, BLOCK_SIZE);
    assert(blkdev_write(mirror, 0, 2, write_buf) == SUCCESS);
    assert(blkdev_read(mirror, 0, 2, read_buf) == SUCCESS);
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);

    blkdev_close(mirror);

    printf("My Mirror Test Passed\n");
}
