#
# file:    Makefile
#
CFLAGS = -g

all: test1 test2 server

%.o: %.c
	${CC} $< -m32 -c -o $@

%.o: %.s
	${AS} --32 $^ -o $@

# $@ refers to the pattern *target* (i.e. 'test1')
# $^ refers to all prerequisites (i.e. 'test1.o qthread.o ...')
test1: test1.o qthread.o stack.o switch.o
	${CC} $^ -m32 -o $@

test2: test2.o qthread.o stack.o switch.o
	${CC} $^ -m32 -o $@

server: server.o qthread.o stack.o switch.o
	${CC} $^ -m32 -o $@

clean:
	rm -f test1 test2 server *.o
