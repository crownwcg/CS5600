#include "blkdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_FAIL -1
#define TEST_SUCCESS 0

/* Write some data to an area of memory */
void write_data(char* data, int length){
    for (int i = 0; i < length; i++){
        data[i] = (char) i;
    }
}

/* Create a new file ready to be used as an image. Every byte of the file will be zero. */
struct blkdev *  create_new_image(char * path, int blocks){
    if (blocks < 1){
        printf("create_new_image: error - blocks must be at least 1: %d\n", blocks);
        return NULL;
    }
    unlink(path);
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

int test_mirror_1(){
    struct blkdev* mirror_drives[2];
    /* Create two images for the mirror */
    mirror_drives[0] = create_new_image("mirror1", 2);
    mirror_drives[1] = create_new_image("mirror2", 2);
    /* Create the raid mirror */
    struct blkdev * mirror = mirror_create(mirror_drives);

    /* Write some data to the mirror, then read the data back and check that the
     * two buffers contain the same bytes.
     */
    char write_buffer[BLOCK_SIZE];
    write_data(write_buffer, BLOCK_SIZE);
    if (blkdev_write(mirror, 0, 1, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    char read_buffer[BLOCK_SIZE];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, BLOCK_SIZE);

    if (blkdev_read(mirror, 0, 1, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }

    /* For debugging, you can analyze these files manually */
    /*
    dump(write_buffer, BLOCK_SIZE, "write-buffer");
    dump(read_buffer, BLOCK_SIZE, "read-buffer");
    */

    if (memcmp(write_buffer, read_buffer, BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    } else {
        return TEST_SUCCESS;
    }
}

int test_mirror_2(){
    struct blkdev* mirror_drives[2];
    /* Create two images for the mirror */
    mirror_drives[0] = create_new_image("mirror1", 10);
    mirror_drives[1] = create_new_image("mirror2", 10);
    /* Create the raid mirror */
    struct blkdev * mirror = mirror_create(mirror_drives);

    char write_buffer[6 * BLOCK_SIZE];
    write_data(write_buffer, 6 * BLOCK_SIZE);
    if (blkdev_write(mirror, 0, 6, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    image_fail(mirror_drives[0]);

    char read_buffer[6 * BLOCK_SIZE];
    bzero(read_buffer, 6 * BLOCK_SIZE);

    if (blkdev_read(mirror, 0, 6, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }
    
    if (memcmp(write_buffer, read_buffer, BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    }

    mirror_replace(mirror, 0, create_new_image("mirror3", 10));
    image_fail(mirror_drives[1]);
    bzero(read_buffer, 6 * BLOCK_SIZE);
    if (blkdev_read(mirror, 0, 6, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }
    if (memcmp(write_buffer, read_buffer, 6 * BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    }

    return TEST_SUCCESS;
}

int test_mirror_3(){
    struct blkdev* mirror_drives[2];
    /* Create two images for the mirror */
    mirror_drives[0] = create_new_image("mirror1", 10);
    mirror_drives[1] = create_new_image("mirror2", 10);
    /* Create the raid mirror */
    struct blkdev * mirror = mirror_create(mirror_drives);

    char write_buffer[6 * BLOCK_SIZE];
    write_data(write_buffer, 6 * BLOCK_SIZE);
    if (blkdev_write(mirror, 0, 6, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    image_fail(mirror_drives[0]);

    char write_buffer2[4 * BLOCK_SIZE];
    write_data(write_buffer2, 4 * BLOCK_SIZE);
    if (blkdev_write(mirror, 6, 4, write_buffer2) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    char read_buffer[10 * BLOCK_SIZE];
    bzero(read_buffer, 10 * BLOCK_SIZE);

    if (blkdev_read(mirror, 0, 10, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }
    
    if (memcmp(write_buffer, read_buffer, 6 * BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    }

    if (memcmp(write_buffer2, read_buffer + 6 * BLOCK_SIZE, 4 * BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    }

    mirror_replace(mirror, 0, create_new_image("mirror3", 10));
    image_fail(mirror_drives[1]);
    bzero(read_buffer, 10 * BLOCK_SIZE);
    if (blkdev_read(mirror, 0, 10, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }
    if (memcmp(write_buffer, read_buffer, 6 * BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    }
    if (memcmp(write_buffer2, read_buffer + 6 * BLOCK_SIZE, 4 * BLOCK_SIZE) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    }

    return TEST_SUCCESS;
}

int test_mirror_4(){
    struct blkdev* mirror_drives[2];
    /* Create two images for the mirror */
    mirror_drives[0] = create_new_image("mirror1", 2);
    mirror_drives[1] = create_new_image("mirror2", 2);
    /* Create the raid mirror */
    struct blkdev * mirror = mirror_create(mirror_drives);
    blkdev_close(mirror);
    return TEST_SUCCESS;
}

const char* test_ok(int status){
    switch (status){
        case TEST_FAIL: return "failed";
        case TEST_SUCCESS: return "passed";
    }
    return "?";
}
    
typedef int (*func)();

void mirror_tests(){
    func tests[] = {test_mirror_1, test_mirror_2, test_mirror_3, test_mirror_4};
    int max = sizeof(tests) / sizeof(func);
    for (int i = 0; i < max; i++){
        printf("Mirror test %d/%d: %s\n", i+1, max, test_ok(tests[i]()));
    }
}

int test_raid0_1(){
    struct blkdev* raid_drives[1];
    int blocks = 10;
    raid_drives[0] = create_new_image("raid1", blocks);
    struct blkdev * raid0 = raid0_create(1, raid_drives, 1);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid0, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    char read_buffer[max_size];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, max_size);

    if (blkdev_read(raid0, 0, blocks, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }

    if (memcmp(write_buffer, read_buffer, max_size) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    } else {
        return TEST_SUCCESS;
    }

    blkdev_close(raid0);
    return TEST_SUCCESS;
}

int test_raid0_2(){
    struct blkdev* raid_drives[1];
    int blocks = 10;
    raid_drives[0] = create_new_image("raid1", blocks);
    struct blkdev * raid0 = raid0_create(1, raid_drives, 2);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid0, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    char read_buffer[max_size];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, max_size);

    if (blkdev_read(raid0, 0, blocks, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }

    if (memcmp(write_buffer, read_buffer, max_size) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    } else {
        return TEST_SUCCESS;
    }

    blkdev_close(raid0);
    return TEST_SUCCESS;
}

int test_raid0_3(){
    struct blkdev* raid_drives[3];
    int blocks = 10;

    /* raid 0-1 -> disk 0, 0-1
     * raid 2-3 -> disk 1, 0-1
     * raid 3-4 -> disk 2, 0-1
     * raid 4-5 -> disk 0, 2-3
     * raid 6-7 -> disk 1, 2-3
     * raid 8-9 -> disk 2, 2-3
     */
    raid_drives[0] = create_new_image("raid1", blocks);
    raid_drives[1] = create_new_image("raid2", blocks);
    raid_drives[2] = create_new_image("raid3", blocks);
    struct blkdev * raid0 = raid0_create(3, raid_drives, 2);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid0, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    char read_buffer[max_size];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, max_size);

    if (blkdev_read(raid0, 0, blocks, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }

    if (memcmp(write_buffer, read_buffer, max_size) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    } else {
        return TEST_SUCCESS;
    }

    blkdev_close(raid0);
    return TEST_SUCCESS;
}

int check_data_n(char* buffer, struct blkdev* disk, int first_block, int blocks){
    char data[BLOCK_SIZE];

    for (int i = 0; i < blocks; i++){
        if (blkdev_read(disk, first_block + i, 1, data) != SUCCESS){
            return 0;
        }

        char * check = buffer + i * BLOCK_SIZE;
        if (memcmp(check, data, BLOCK_SIZE) != 0){
            return 0;
        }
    }

    return 1;
}

int check_data(char* buffer, int raid_lba, struct blkdev* disk, int lba){
    char * check = buffer + raid_lba * BLOCK_SIZE;

    char data[BLOCK_SIZE];
    if (blkdev_read(disk, lba, 1, data) != SUCCESS){
        return 0;
    }

    if (memcmp(check, data, BLOCK_SIZE) != 0){
        return 0;
    }

    return 1;
}

static void xor(char *dst, const char *src, int len){
    int i;
    for (i = 0; i < len; i++)
        dst[i] = dst[i] ^ src[i];
}

int check_parity(struct blkdev** disks, int count, int lba){
    char data[BLOCK_SIZE];
    bzero(data, BLOCK_SIZE);
    for (int i = 0; i < count - 1; i++){
        char buffer[BLOCK_SIZE];
        if (blkdev_read(disks[i], lba, 1, buffer) != SUCCESS){
            return 0;
        }
        xor(data, buffer, BLOCK_SIZE);
    }

    char parity_data[BLOCK_SIZE];
    if (blkdev_read(disks[count-1], lba, 1, parity_data) != SUCCESS){
        return 0;
    }
    if (memcmp(data, parity_data, BLOCK_SIZE) != 0){
        return 0;
    }

    return 1;
}

int test_raid0_4(){
    struct blkdev* raid_drives[3];
    int blocks = 10;

    raid_drives[0] = create_new_image("raid1", blocks);
    raid_drives[1] = create_new_image("raid2", blocks);
    raid_drives[2] = create_new_image("raid3", blocks);
    struct blkdev * raid0 = raid0_create(3, raid_drives, 2);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid0, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    /* raid 0-1 -> disk 0, 0-1
     * raid 2-3 -> disk 1, 0-1
     * raid 4-5 -> disk 2, 0-1
     * raid 6-7 -> disk 0, 2-3
     * raid 8-9 -> disk 1, 2-3
     */
    if (!check_data(write_buffer, 0, raid_drives[0], 0)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 1, raid_drives[0], 1)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 2, raid_drives[1], 0)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 3, raid_drives[1], 1)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 4, raid_drives[2], 0)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 5, raid_drives[2], 1)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 6, raid_drives[0], 2)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 7, raid_drives[0], 3)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 8, raid_drives[1], 2)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 9, raid_drives[1], 3)){
        return TEST_FAIL;
    }

    blkdev_close(raid0);
    return TEST_SUCCESS;
}

void raid0_tests(){
    func tests[] = {test_raid0_1, test_raid0_2, test_raid0_3, test_raid0_4};
    int max = sizeof(tests) / sizeof(func);
    for (int i = 0; i < max; i++){
        printf("Raid 0 test %d/%d: %s\n", i+1, max, test_ok(tests[i]()));
    }
}

int test_raid4_1(){
    struct blkdev* raid_drives[3];
    int blocks = 10;

    /* raid 0-1 -> disk 0, 0-1
     * raid 2-3 -> disk 1, 0-1
     * raid 3-4 -> disk 2, 0-1
     * raid 4-5 -> disk 0, 2-3
     * raid 6-7 -> disk 1, 2-3
     * raid 8-9 -> disk 2, 2-3
     */
    raid_drives[0] = create_new_image("raid1", blocks);
    raid_drives[1] = create_new_image("raid2", blocks);
    raid_drives[2] = create_new_image("raid3", blocks);
    struct blkdev * raid4 = raid4_create(3, raid_drives, 2);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid4, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    char read_buffer[max_size];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, max_size);

    if (blkdev_read(raid4, 0, blocks, read_buffer) != SUCCESS){
        printf("Read failed!\n");
        return TEST_FAIL;
    }

    if (memcmp(write_buffer, read_buffer, max_size) != 0){
        printf("Read doesn't match write!\n");
        return TEST_FAIL;
    } else {
        return TEST_SUCCESS;
    }
    
    if (!check_parity(raid_drives, 3, 0)){
        return TEST_FAIL;
    }

    blkdev_close(raid4);
    return TEST_SUCCESS;
}

int test_raid4_2(){
    struct blkdev* raid_drives[3];
    int blocks = 10;

    raid_drives[0] = create_new_image("raid1", blocks);
    raid_drives[1] = create_new_image("raid2", blocks);
    raid_drives[2] = create_new_image("raid3", blocks);
    struct blkdev * raid4 = raid4_create(3, raid_drives, 2);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid4, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    /* raid 0-1 -> disk 0, 0-1
     * raid 2-3 -> disk 1, 0-1
     * raid 4-5 -> disk 0, 2-3
     * raid 6-7 -> disk 1, 2-3
     * raid 8-9 -> disk 0, 4-5
     */
    if (!check_data(write_buffer, 0, raid_drives[0], 0)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 1, raid_drives[0], 1)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 2, raid_drives[1], 0)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 3, raid_drives[1], 1)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 4, raid_drives[0], 2)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 5, raid_drives[0], 3)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 6, raid_drives[1], 2)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 7, raid_drives[1], 3)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 8, raid_drives[0], 4)){
        return TEST_FAIL;
    }
    if (!check_data(write_buffer, 9, raid_drives[0], 5)){
        return TEST_FAIL;
    }

    blkdev_close(raid4);
    return TEST_SUCCESS;
}

int check_equal_files(const char* path1, const char* path2){
    FILE * f1 = fopen(path1, "r");
    FILE * f2 = fopen(path2, "r");

    if (!f1 || !f2){
        return 0;
    }

    char buffer1[1024];
    char buffer2[1024];

    int ok = 1;
    while (!feof(f1)){
        int bytes1 = fread(buffer1, 1, 1024, f1);
        int bytes2 = fread(buffer2, 1, 1024, f2);
        if (bytes1 != bytes2){
            ok = 0;
            break;
        }
        if (memcmp(buffer1, buffer2, bytes1) != 0){
            ok = 0;
            break;
        }
    }

    fclose(f1);
    fclose(f2);

    return ok;
}

int test_raid4_3(){
    struct blkdev* raid_drives[3];
    int blocks = 10;

    raid_drives[0] = create_new_image("raid1", blocks);
    raid_drives[1] = create_new_image("raid2", blocks);
    raid_drives[2] = create_new_image("raid3", blocks);
    struct blkdev * raid4 = raid4_create(3, raid_drives, 2);
    int max_size = blocks * BLOCK_SIZE;

    char write_buffer[max_size];
    write_data(write_buffer, max_size);
    if (blkdev_write(raid4, 0, blocks, write_buffer) != SUCCESS){
        printf("Write failed!\n");
        return TEST_FAIL;
    }

    for (int disk = 0; disk < 3; disk++){
        image_fail(raid_drives[disk]);

        /* Read in degraded mode */

        if (!check_data_n(write_buffer, raid4, 0, blocks)){
            return TEST_FAIL;
        }

        /* Force parity to be rewritten in case it was ignored from the read */
        if (blkdev_write(raid4, 0, 1, write_buffer) != SUCCESS){
            return TEST_FAIL;
        }
        
        if (raid4_replace(raid4, disk, create_new_image("raidX", blocks)) != SUCCESS){
            return TEST_FAIL;
        }

        /* Check that the parity is still right */
        if (!check_parity(raid_drives, 3, 0) != SUCCESS){
            return TEST_FAIL;
        }

        /* Should be a read in non-degraded mode */
        if (!check_data_n(write_buffer, raid4, 0, blocks)){
            return TEST_FAIL;
        }
    }

    blkdev_close(raid4);
    return TEST_SUCCESS;
}

void raid4_tests(){
    func tests[] = {test_raid4_1, test_raid4_2, test_raid4_3};
    int max = sizeof(tests) / sizeof(func);
    for (int i = 0; i < max; i++){
        printf("Raid 4 test %d/%d: %s\n", i+1, max, test_ok(tests[i]()));
    }
}

int main(){
    mirror_tests();
    raid0_tests();
    raid4_tests();
}