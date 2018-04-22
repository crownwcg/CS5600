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
    // Passes all other tests with different strip sizes (e.g. 2, 4, 7, and 32 sectors) 
    // and different numbers of disks.
    int units[] = {2, 4, 7, 32};
    int ndisks[] = {4, 7, 10};
    char *img_names[10] = {
        "image1", "image2", "image3", "image4", "image5",
        "image6", "image7", "image8", "image9", "image10",
    };
    struct blkdev *raid0;
    srand(time(NULL));
    int unit, ndisk, nblks;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            unit  = units[i];
            ndisk = ndisks[j];

            // create disks
            struct blkdev *disks[ndisk];
            for (int k = 0; k < ndisk; k++) {
                disks[k] = create_new_image(img_names[k], BLK_NUM);
            }

            // create raid0
            raid0 = raid0_create(ndisk, disks, unit);
            nblks = blkdev_num_blocks(raid0);

            // reports the correct size
            assert(raid0 != NULL);
            assert(nblks == blkdev_num_blocks(disks[0]) / unit * unit * ndisk);

            // read/write cross stripe of the raid0
            char read_buf[BLOCK_SIZE * 2];
            char write_buf[BLOCK_SIZE * 2];
            write_data(write_buf, BLOCK_SIZE * 2);

            // random test
            char backup[BLOCK_SIZE * nblks];
            bzero(backup, BLOCK_SIZE * nblks);
            assert(blkdev_write(raid0, 0, nblks, backup) == SUCCESS);

            // reads data from the right disks and locations. 
            // overwrites the correct locations. 
            for (int k = 0; k < TESTS_NUM; k++) {
                int first_blk = rand() % nblks - 1;
                if (first_blk < 0) continue;
                assert(blkdev_write(raid0, first_blk, 2, write_buf) == SUCCESS);
                memcpy(backup + first_blk * BLOCK_SIZE, write_buf, BLOCK_SIZE * 2);
                assert(blkdev_read(raid0, first_blk, 2, read_buf) == SUCCESS);
                assert(memcmp(write_buf, read_buf, BLOCK_SIZE * 2) == 0);
                bzero(read_buf, BLOCK_SIZE * 2);
            }

            char image_copy[BLOCK_SIZE * nblks];
            assert(blkdev_read(raid0, 0, nblks, image_copy) == SUCCESS);
            assert(memcpy(backup, image_copy, BLOCK_SIZE * nblks));

            // fail a disk and verify that the volume fails.
            image_fail(disks[0]);
            assert(blkdev_write(raid0, 0, 1, write_buf) != SUCCESS);
            assert(blkdev_read(raid0, 0, 1, read_buf) != SUCCESS);

            // close
            blkdev_close(raid0);
        }
    }

    printf("Raid0 Test All Passed\n");
}
