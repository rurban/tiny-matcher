CC := cc
#CFLAGS := -O3 -Wall -Wextra
CFLAGS := -g -Wall -Wextra -std=c99 -DDEBUG

all: test1

test1: match.c
	$(CC) -I. $(CFLAGS) match.c -DTEST -o $@

test: all 
	@./test1

CBMC := cbmc
# unwindset: loop max 16 patterns
# --enum-range-check not with cbmc 5.10 on ubuntu-latest
verify:
	$(CBMC) -DCPROVER -DTEST --unwindset 8 --unwind 16 --depth 16 --bounds-check --pointer-check --memory-leak-check --div-by-zero-check --signed-overflow-check --unsigned-overflow-check --pointer-overflow-check --conversion-check --undefined-shift-check $(CBMC_ARGS) match.c

clean:
	@rm -f test1
	@rm -f a.out
	@rm -f *.o
