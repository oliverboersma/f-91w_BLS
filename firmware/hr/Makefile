# Host-side test for the heart-rate algorithm (no hardware needed)
CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra

test: test_hr
	./test_hr

test_hr: test_hr.c hr_spotcheck.c hr_spotcheck.h
	$(CC) $(CFLAGS) -o $@ test_hr.c hr_spotcheck.c -lm

clean:
	rm -f test_hr
