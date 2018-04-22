make clean
make 

dotest() {
    # $1 test name
    # $2 cmd
    # $3 expected output
    NAME="$1"
    CMD="$2"
    EXPECTED="$3"

    ./mktest test.img

    echo
    echo "Tesing $NAME"
    ACTUAL=$(eval $CMD)

    if [ "$ACTUAL" != "$EXPECTED" ]; then
        echo
        echo "**** FAILED $NAME ****"
        echo "** Diff **"
        diff <(echo "$ACTUAL") <(echo "$EXPECTED")
        exit 1
    fi

    echo "Tesing $NAME passed"
    rm test.img
}

HW="./homework"

dotest "chmod" \
    "printf '%s\n' 'chmod 777 dir1' 'ls-l' 'chmod 666 /dir1/file.0' 'ls-l /dir1' | $HW -cmdline -image test.img" \
    "read/write block size: 1000
cmd> chmod 777 dir1
cmd> ls-l
dir1 drwxrwxrwx 0 0 Fri Jul 13 11:08:00 2012
file.7 -rwxrwxrwx 6644 7 Fri Jul 13 11:06:20 2012
file.A -rwxrwxrwx 1000 1 Fri Jul 13 11:04:40 2012
cmd> chmod 666 /dir1/file.0
cmd> ls-l /dir1
file.0 -rw-rw-rw- 0 0 Fri Jul 13 11:04:40 2012
file.2 -rwxrwxrwx 2012 2 Fri Jul 13 11:04:40 2012
file.270 -rwxrwxrwx 276177 270 Fri Jul 13 11:06:20 2012
cmd> "


dotest "mkdir" \
    "printf '%s\n' 'mkdir a' 'ls' 'mkdir a' 'ls' | $HW -cmdline -image test.img" \
    "read/write block size: 1000
cmd> mkdir a
cmd> ls
a
dir1
file.7
file.A
cmd> mkdir a
error: File exists
cmd> ls
a
dir1
file.7
file.A
cmd> "

dotest "rmdir rm rename" \
    "printf '%s\n' 'rmdir dir1' 'rm file.7' 'rename file.A file.B' 'ls' 'rm file.A' 'rename file.B file.A' 'ls'| $HW -cmdline -image test.img" \
    "read/write block size: 1000
cmd> rmdir dir1
error: Directory not empty
cmd> rm file.7
cmd> rename file.A file.B
cmd> ls
dir1
file.B
cmd> rm file.A
error: No such file or directory
cmd> rename file.B file.A
cmd> ls
dir1
file.A
cmd> "

dotest "read show truncate" \
    "printf '%s\n' 'ls-l' 'show file.A' 'truncate file.A' 'ls-l' 'rm file.A' 'ls-l' | $HW -cmdline -image test.img" \
    "read/write block size: 1000
cmd> ls-l
dir1 drwxr-xr-x 0 0 Fri Jul 13 11:08:00 2012
file.7 -rwxrwxrwx 6644 7 Fri Jul 13 11:06:20 2012
file.A -rwxrwxrwx 1000 1 Fri Jul 13 11:04:40 2012
cmd> show file.A
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAcmd> truncate file.A
cmd> ls-l
dir1 drwxr-xr-x 0 0 Fri Jul 13 11:08:00 2012
file.7 -rwxrwxrwx 6644 7 Fri Jul 13 11:06:20 2012
file.A -rwxrwxrwx 0 0 Fri Jul 13 11:04:40 2012
cmd> rm file.A
cmd> ls-l
dir1 drwxr-xr-x 0 0 Fri Jul 13 11:08:00 2012
file.7 -rwxrwxrwx 6644 7 Fri Jul 13 11:06:20 2012
cmd> "

dotest "read write large files" \
    "printf '%s\n' 'chmod 777 dir1/file.270' 'get /dir1/file.270 /tmp/file.270' 'put /tmp/file.270 file.270' 'ls' | $HW -cmdline -image test.img" \
    "read/write block size: 1000
cmd> chmod 777 dir1/file.270
cmd> get /dir1/file.270 /tmp/file.270
cmd> put /tmp/file.270 file.270
cmd> ls
dir1
file.270
file.7
file.A
cmd> "

dotest "read write large files blksize=800" \
    "printf '%s\n' 'blksiz 800' 'chmod 777 dir1/file.270' 'get /dir1/file.270 /tmp/file.270' 'put /tmp/file.270 file.270' 'ls' | $HW -cmdline -image test.img" \
    "read/write block size: 1000
cmd> blksiz 800
read/write block size: 800
cmd> chmod 777 dir1/file.270
cmd> get /dir1/file.270 /tmp/file.270
cmd> put /tmp/file.270 file.270
cmd> ls
dir1
file.270
file.7
file.A
cmd> "

echo "ALL TESTS DONE"
make clean
