/*
 * file:        homework.c
 * description: skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern Computer Science, 2011
 * $Id: homework.c 410 2011-11-07 18:42:45Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "blkdev.h"

/********** MIRRORING ***************/

/* example state for mirror device. See mirror_create for how to
 * initialize a struct blkdev with this.
 */
struct mirror_dev {
    struct blkdev *disks[2];    /* flag bad disk by setting to NULL */
    int nblks;                  /* num of blocks of mirror dev */
};
    
static int mirror_num_blocks(struct blkdev *dev) {
    struct mirror_dev *mirror = (struct mirror_dev*) dev->private;
    return mirror->nblks;
}

/* read from one of the sides of the mirror. (if one side has failed,
 * it had better be the other one...) If both sides have failed,
 * return an error.
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should close the
 * device and flag it (e.g. as a null pointer) so you won't try to use
 * it again. 
 */
static int mirror_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    // get two sides
    struct mirror_dev *mdev = (struct mirror_dev *) dev->private;
    struct blkdev     *side[2];
    side[0] = mdev->disks[0];
    side[1] = mdev->disks[1];
    
    // read two sides
    int val = E_UNAVAIL;

    if (side[0] != NULL) {
        val = side[0]->ops->read(side[0], first_blk, num_blks, buf);
        if (val == E_UNAVAIL) {
            side[0]->ops->close(side[0]);
            mdev->disks[0] = NULL;
        } 

        if (val == SUCCESS) {
            return SUCCESS;
        }
    }

    if (side[1] != NULL) {
        val = side[1]->ops->read(side[1], first_blk, num_blks, buf);
        if (val == E_UNAVAIL) {
            side[1]->ops->close(side[1]);
            mdev->disks[1] = NULL;
        } 
    }

    return val;
}

/* write to both sides of the mirror, or the remaining side if one has
 * failed. If both sides have failed, return an error.
 * Note that a write operation may indicate that the underlying device
 * has failed, in which case you should close the device and flag it
 * (e.g. as a null pointer) so you won't try to use it again.
 */
static int mirror_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf)
{
    // get two sides
    struct mirror_dev *mdev = (struct mirror_dev*) dev->private;
    struct blkdev     *side[2];
    side[0] = mdev->disks[0];
    side[1] = mdev->disks[1];

    // write two sides
    int val = E_UNAVAIL;
    if (side[0] != NULL) {
        val &= side[0]->ops->write(side[0], first_blk, num_blks, buf);
        if (val == E_UNAVAIL) {
            side[0]->ops->close(side[0]);
            mdev->disks[0] = NULL;
        } 
    }

    if (side[1] != NULL) {
        val &= side[1]->ops->write(side[1], first_blk, num_blks, buf);
        if (val == E_UNAVAIL) {
            side[1]->ops->close(side[1]);
            mdev->disks[1] = NULL;
        }
    }

    return val;
}

/* clean up, including: close any open (i.e. non-failed) devices, and
 * free any data structures you allocated in mirror_create.
 */
static void mirror_close(struct blkdev *dev)
{
    // get two sides
    struct mirror_dev *mdev = (struct mirror_dev*) dev->private;

    // close disk sides
    if (mdev->disks[0] != NULL) {
        mdev->disks[0]->ops->close(mdev->disks[0]);
        mdev->disks[0] = NULL;
    }
    if (mdev->disks[1] != NULL) {
        mdev->disks[1]->ops->close(mdev->disks[1]);
        mdev->disks[1] = NULL;
    }

    // free for memory allocation
    free(mdev);
    free(dev);
}

struct blkdev_ops mirror_ops = {
    .num_blocks = mirror_num_blocks,
    .read       = mirror_read,
    .write      = mirror_write,
    .close      = mirror_close
};

/* create a mirrored volume from two disks. Do not write to the disks
 * in this function - you should assume that they contain identical
 * contents. 
 */
struct blkdev *mirror_create(struct blkdev *disks[2])
{
    // If the devices are not the same size, print an error message and return NULL.
    if (disks[0]->ops->num_blocks(disks[0]) != disks[1]->ops->num_blocks(disks[1])) {
        printf("Error: mirror disks have different size.\n");
        return NULL;
    }

    struct blkdev *dev      = (struct blkdev *)     malloc(sizeof(*dev));
    struct mirror_dev *mdev = (struct mirror_dev *) malloc(sizeof(*mdev));

    // initiate fields of the mirror dev.
    mdev->disks[0] = disks[0];
    mdev->disks[1] = disks[1];
    mdev->nblks    = disks[0]->ops->num_blocks(disks[0]);

    dev->private = (void *) mdev;
    dev->ops     = &mirror_ops;
    return dev;
}

/* replace failed device 'i' (0 or 1) in a mirror. Note that we assume
 * the upper layer knows which device failed. You will need to
 * replicate content from the other underlying device before returning
 * from this call.
 */
int mirror_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    // get the other underlying device
    struct mirror_dev *mdev = (struct mirror_dev*) volume->private;
    int                side = 1 - i;
    
    // If the new disk is not the same size as the old one, return E_SIZE.
    if (mdev->nblks != newdisk->ops->num_blocks(newdisk)) {
        return E_SIZE;
    }

    // read the volume and write the data to the new disk
    char buffer[BLOCK_SIZE];
    int  val;
    for (int block_num = 0; block_num < mdev->nblks; block_num++) {
        // check the status of the I/O functions
        val = mdev->disks[side]->ops->read(mdev->disks[side], block_num, 1, buffer);
        if (val != SUCCESS) {
            return val;
        }

        val = newdisk->ops->write(newdisk, block_num, 1, buffer);
        if (val == E_UNAVAIL) {
            newdisk->ops->close(newdisk);
            newdisk = NULL;
        } 

        if (val != SUCCESS) {
            printf("Error: unable to replace disk.\n");
            return val;
        }
    }

    // point to the new disk
    mdev->disks[i] = newdisk;
    return SUCCESS;
}

/**********  RAID0 ***************/

typedef struct raid0_dev {
    struct blkdev **disks;  // array of blkdev pointer
    int             ndisks; // number of disks
    int             nblks;  // number of blocks
    int             unit;   // stripe's unit
} raid0_dev;

static int raid0_num_blocks(struct blkdev *dev)
{   
    raid0_dev *r0_dev = (raid0_dev *) dev->private;
    return r0_dev->nblks;
}

/* read blocks from a striped volume. 
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should (a) close the
 * device and (b) return an error on this and all subsequent read or
 * write operations. 
 */
static int raid0_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    // get raid0 dev
    raid0_dev     *r0_dev = (raid0_dev *) dev->private;
    int            ndisks = r0_dev->ndisks,
                   unit   = r0_dev->unit,
                   val,                 // status of r/w functions
                   nth_disk,            // nth disk of the block i
                   nth_stripe_of_disk,  // nth stripe of current disk
                   blk_num_in_disk;     // number of block in current disk
    struct blkdev *d;                   // current disk
    void          *buf_offset;          // buffer offeset
    for (int i = first_blk; i < first_blk + num_blks; i++) {
        nth_disk           = i / unit % ndisks;
        nth_stripe_of_disk = i / unit / ndisks;
        d                  = r0_dev->disks[nth_disk];
        buf_offset         = buf + (i - first_blk) * BLOCK_SIZE;
        blk_num_in_disk    = nth_stripe_of_disk * unit + i % unit;

        if (d == NULL) {
            return E_UNAVAIL;
        }

        val = d->ops->read(d, blk_num_in_disk, 1, buf_offset);
        // close dev if unavailable
        if (val == E_UNAVAIL) {
            d->ops->close(d);
            r0_dev->disks[nth_disk] = NULL;
        } 

        if (val != SUCCESS) {
            return val;
        }
    }

    return SUCCESS;
}

/* write blocks to a striped volume.
 * Again if an underlying device fails you should close it and return
 * an error for this and all subsequent read or write operations.
 */
static int raid0_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf)
{
    // get raid0 dev
    raid0_dev     *r0_dev = (raid0_dev *) dev->private;
    int            ndisks = r0_dev->ndisks,
                   unit   = r0_dev->unit,
                   val,                 // status of r/w functions
                   nth_disk,            // nth disk of the block i
                   nth_stripe_of_disk,  // nth stripe of current disk
                   blk_num_in_disk;     // number of block in current disk
    struct blkdev *d;                   // current disk
    void          *buf_offset;          // buffer offeset
    for (int i = first_blk; i < first_blk + num_blks; i++) {
        nth_disk           = i / unit % ndisks;
        nth_stripe_of_disk = i / unit / ndisks;
        d                  = r0_dev->disks[nth_disk];
        buf_offset         = buf + (i - first_blk) * BLOCK_SIZE;
        blk_num_in_disk    = nth_stripe_of_disk * unit + i % unit;
        
        if (d == NULL) {
            return E_UNAVAIL;
        }

        val = d->ops->write(d, blk_num_in_disk, 1, buf_offset);
        // close dev if unavailable
        if (val == E_UNAVAIL) {
            d->ops->close(d);
            r0_dev->disks[nth_disk] = NULL;
        } 

        if (val != SUCCESS) {
            return val;
        }
    }

    return SUCCESS;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in stripe_create. 
 */
static void raid0_close(struct blkdev *dev)
{
    // get raid0 dev
    raid0_dev *r0_dev = (raid0_dev *) dev->private;

    // close disks
    for (int i = 0; i < r0_dev->ndisks; i++) {
        if (r0_dev->disks[i] != NULL) {
            r0_dev->disks[i]->ops->close(r0_dev->disks[i]);
            r0_dev->disks[i] = NULL;
        }
    }

    // free allocated memory
    free(r0_dev->disks);
    free(r0_dev);
    free(dev);
}

struct blkdev_ops raid0_ops = {
    .num_blocks = raid0_num_blocks,
    .read       = raid0_read,
    .write      = raid0_write,
    .close      = raid0_close
};

/* create a striped volume across N disks, with a stripe size of
 * 'unit'. (i.e. if 'unit' is 4, then blocks 0..3 will be on disks[0],
 * 4..7 on disks[1], etc.)
 * Check the size of the disks to compute the final volume size, and
 * fail (return NULL) if they aren't all the same.
 * Do not write to the disks in this function.
 */
struct blkdev *raid0_create(int N, struct blkdev *disks[], int unit)
{   
    // Check the size of the disks to compute the final volume size, and
    // fail (return NULL) if they aren't all the same.
    int size_of_disks = disks[0]->ops->num_blocks(disks[0]);
    for (int i = 1; i < N; i++) {
        if (disks[i]->ops->num_blocks(disks[i]) != size_of_disks) {
            printf("Error: the size of disks is not the same.\n");
            return NULL;
        }
    }

    // memory allocation and initiate the fields
    struct blkdev *dev = (struct blkdev *)  malloc(sizeof(*dev));
    raid0_dev *r0_dev  = (raid0_dev *)      malloc(sizeof(*r0_dev));
    r0_dev->disks      = (struct blkdev **) malloc(sizeof(*dev) * N);
    r0_dev->ndisks     = N;
    r0_dev->nblks      = size_of_disks / unit * unit * N;
    r0_dev->unit       = unit;
    for (int i = 0; i < N; i++) {
        r0_dev->disks[i] = disks[i];
    }

    dev->private = (void *) r0_dev;
    dev->ops     = &raid0_ops;
    return dev;
}

/**********   RAID 4  ***************/

#define DEFAULT -1
#define FALSE 0
#define TRUE 1

typedef struct raid4_dev {
    struct blkdev **disks;          // array of blkdev pointer
    int             last_failed;    // last failed disk number
    int             ndisks;         // number of disks
    int             nblks;          // number of blocks
    int             unit;           // stripe's unit
} raid4_dev;

/* helper function - compute parity function across two blocks of
 * 'len' bytes and put it in a third block. Note that 'dst' can be the
 * same as either 'src1' or 'src2', so to compute parity across N
 * blocks you can do: 
 *
 *     void **block[i] - array of pointers to blocks
 *     dst = <zeros[len]>
 *     for (i = 0; i < N; i++)
 *        parity(block[i], dst, dst);
 *
 * Yes, it could be faster. Don't worry about it.
 */
void parity(int len, void *src1, void *src2, void *dst)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i;
    for (i = 0; i < len; i++)
        d[i] = s1[i] ^ s2[i];
}

/* get number of blocks in raid4 device
 */
static int raid4_num_blocks(struct blkdev *dev)
{   
    raid4_dev *r4_dev = (raid4_dev *) dev->private;
    return r4_dev->nblks;
}

/* recover data from failed block
 */
static int recover_data(raid4_dev *r4_dev, void *buf, int blk_num) {
    int val, isFirst = TRUE;
    char rec[BLOCK_SIZE];
    char tmp[BLOCK_SIZE];
    for (int i = 0; i < r4_dev->ndisks; i++) {
        if (i == r4_dev->last_failed) {
            continue;
        }
        if (isFirst) {
            val = r4_dev->disks[i]->ops->read(r4_dev->disks[i], blk_num, 1, rec);
            if (val != SUCCESS) {
                return val;
            }
            isFirst = FALSE;
        } else {
            val = r4_dev->disks[i]->ops->read(r4_dev->disks[i], blk_num, 1, tmp);
            if (val != SUCCESS) {
                return val;
            }
            parity(BLOCK_SIZE, rec, tmp, rec);
        }
    }

    memcpy(buf, rec, BLOCK_SIZE);
    return SUCCESS;
}

/* check status of raid4 read
 */
static int r4_read_check(struct blkdev *d, raid4_dev *r4_dev, 
                         int nth_disk, int blk_num_in_disk, void *buf_offset) {
    int val = d->ops->read(d, blk_num_in_disk, 1, buf_offset);
    if (val == E_UNAVAIL) {
        if (r4_dev->last_failed != DEFAULT && 
            r4_dev->last_failed != nth_disk) {
            d->ops->close(d);
            r4_dev->disks[nth_disk] = NULL;
            return E_UNAVAIL;
        } 

        r4_dev->last_failed = nth_disk;
        val = recover_data(r4_dev, buf_offset, blk_num_in_disk);
    } 
    return val;
}

/* read blocks from a RAID 4 volume.
 * If the volume is in a degraded state you may need to reconstruct
 * data from the other stripes of the stripe set plus parity.
 * If a drive fails during a read and all other drives are
 * operational, close that drive and continue in degraded state.
 * If a drive fails and the volume is already in a degraded state,
 * close the drive and return an error.
 */
static int raid4_read(struct blkdev * dev, int first_blk,
                      int num_blks, void *buf) 
{
    // get raid4 dev and declear variables
    raid4_dev     *r4_dev = (raid4_dev *) dev->private;
    int            ndisks = r4_dev->ndisks,
                   unit   = r4_dev->unit,
                   val,                 // status of operation
                   nth_disk,            // nth disk of the block i
                   nth_stripe_of_disk,  // stripe index in the current disk
                   blk_num_in_disk;     // block index in the current disk
    struct blkdev *d;                   // current disk
    void          *buf_offset;          // offset to the parsing buffer 

    for (int i = first_blk; i < first_blk + num_blks; i++) {
        nth_disk           = i / unit % (ndisks - 1); 
        nth_stripe_of_disk = i / unit / (ndisks - 1); 
        blk_num_in_disk    = nth_stripe_of_disk * unit + i % unit;
        buf_offset         = buf + (i - first_blk) * BLOCK_SIZE;
        d                  = r4_dev->disks[nth_disk];

        val = r4_read_check(d, r4_dev, nth_disk, blk_num_in_disk, buf_offset);
        if (val != SUCCESS) {
            return val;
        }
    }

    return SUCCESS;
}

/* check status of raid4 write
 */
static int r4_write_check(struct blkdev *d, raid4_dev *r4_dev, 
                          int nth_disk, int blk_num_in_disk, void *buf_offset) {
    int val = d->ops->write(d, blk_num_in_disk, 1, buf_offset);
    if (val == E_UNAVAIL) {
        if (r4_dev->last_failed != DEFAULT && 
            r4_dev->last_failed != nth_disk) {
            d->ops->close(d);
            r4_dev->disks[nth_disk] = NULL;
            return E_UNAVAIL;
        }
        r4_dev->last_failed = nth_disk;
    } 
    return val;
}

/* write blocks to a RAID 4 volume.
 * Note that you must handle short writes - i.e. less than a full
 * stripe set. You may either use the optimized algorithm (for N>3
 * read old data, parity, write new data, new parity) or you can read
 * the entire stripe set, modify it, and re-write it. Your code will
 * be graded on correctness, not speed.
 * If an underlying device fails you should close it and complete the
 * write in the degraded state. If a drive fails in the degraded
 * state, close it and return an error.
 * In the degraded state perform all writes to non-failed drives, and
 * forget about the failed one. (parity will handle it)
 */
static int raid4_write(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    // get raid4 dev and declear variables
    raid4_dev     *r4_dev = (raid4_dev *) dev->private;
    int            ndisks = r4_dev->ndisks,
                   unit   = r4_dev->unit,
                   val,                 // status of operation
                   nth_disk,            // nth disk of the block i
                   nth_stripe_of_disk,  // stripe index in the current disk
                   blk_num_in_disk;     // block index in the current disk
    struct blkdev *d;                   // current disk
    void          *buf_offset;          // offset to the parsing buffer
    struct blkdev *pd     = r4_dev->disks[ndisks - 1];  // parity disk

    for (int i = first_blk; i < first_blk + num_blks; i++) {
        nth_disk           = i / unit % (ndisks - 1);
        nth_stripe_of_disk = i / unit / (ndisks - 1);
        blk_num_in_disk    = nth_stripe_of_disk * unit + i % unit;
        buf_offset         = buf + (i - first_blk) * BLOCK_SIZE;
        d                  = r4_dev->disks[nth_disk];

        char old_data[BLOCK_SIZE], 
             old_parity[BLOCK_SIZE];

        // read nth disk's data, if it fails, recover it from parity disk
        val = r4_read_check(d, r4_dev, nth_disk, blk_num_in_disk, old_data);
        if (val != SUCCESS) {
            return val;
        }

        // read from parity
        val = r4_read_check(pd, r4_dev, ndisks - 1, blk_num_in_disk, old_parity);
        if (val != SUCCESS) {
            return val;
        }

        // generate parity
        parity(BLOCK_SIZE, old_data, buf_offset, old_data);
        parity(BLOCK_SIZE, old_data, old_parity, old_parity);

        // write to nth disk, if it fails, just write to parity
        // if one has succeeded, write succeeds.
        val = r4_write_check(d , r4_dev, nth_disk  , blk_num_in_disk, buf_offset) &
              r4_write_check(pd, r4_dev, ndisks - 1, blk_num_in_disk, old_parity);

        if (val != SUCCESS) {
            return val;
        }
    }

    return SUCCESS;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in raid4_create. 
 */
static void raid4_close(struct blkdev *dev)
{
    // get raid0=4 dev
    raid4_dev *r4_dev = (raid4_dev *) dev->private;

    // close disks
    for (int i = 0; i < r4_dev->ndisks; i++) {
        if (r4_dev->disks[i] != NULL) {
            r4_dev->disks[i]->ops->close(r4_dev->disks[i]);
            r4_dev->disks[i] = NULL;
        }
    }

    // free allocated memory
    free(r4_dev->disks);
    free(r4_dev);
    free(dev);
}

struct blkdev_ops raid4_ops = {
    .num_blocks = raid4_num_blocks,
    .read       = raid4_read,
    .write      = raid4_write,
    .close      = raid4_close
};

/* Initialize a RAID 4 volume with strip size 'unit', using
 * disks[N-1] as the parity drive. Do not write to the disks - assume
 * that they are properly initialized with correct parity. (warning -
 * some of the grading scripts may fail if you modify data on the
 * drives in this function)
 */
struct blkdev *raid4_create(int N, struct blkdev *disks[], int unit)
{
    // Check the size of the disks to compute the final volume size, and
    // fail (return NULL) if they aren't all the same.
    int size_of_disks = disks[0]->ops->num_blocks(disks[0]);
    for (int i = 1; i < N; i++) {
        if (disks[i]->ops->num_blocks(disks[i]) != size_of_disks) {
            printf("Error: the size of disks is not the same.\n");
            return NULL;
        }
    }

    // memory allocation and initiate the fields
    struct blkdev *dev    = (struct blkdev *) malloc(sizeof(*dev));
    raid4_dev     *r4_dev = (raid4_dev *)     malloc(sizeof(*r4_dev));    
    r4_dev->last_failed   = DEFAULT;      
    r4_dev->ndisks        = N;
    r4_dev->nblks         = size_of_disks / unit * unit * (N - 1);
    r4_dev->unit          = unit;

    // memory allocation for the disks pointer array
    r4_dev->disks       = (struct blkdev **) malloc(N * sizeof(*dev));  
    for (int i = 0; i < N; i++) {
        r4_dev->disks[i] = disks[i];
    }

    dev->private = (void *) r4_dev;
    dev->ops     = &raid4_ops;
    return dev;
}

/* replace failed device 'i' in a RAID 4. Note that we assume
 * the upper layer knows which device failed. You will need to
 * reconstruct content from data and parity before returning
 * from this call.
 */
int raid4_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    // get raid0=4 dev
    raid4_dev *r4_dev = (raid4_dev *) volume->private;
    int        ndisks = r4_dev->ndisks;
    int        nblks  = r4_dev->nblks;

    if (r4_dev->disks[0]->ops->num_blocks(r4_dev->disks[0]) != 
        newdisk->ops->num_blocks(newdisk)) {
        return E_SIZE;
    }

    int val, blk_size = nblks / (ndisks - 1);
    char recover[BLOCK_SIZE];
    // replace data
    for (int k = 0; k < blk_size; k++) {
        val = recover_data(r4_dev, (void *) recover, k);
        if (val != SUCCESS) {
            return val;
        }

        val = newdisk->ops->write(newdisk, k, 1, recover);
        if (val != SUCCESS) {
            return val;
        }
    }

    r4_dev->disks[i] = newdisk;
    r4_dev->last_failed = DEFAULT;
    return SUCCESS;
}