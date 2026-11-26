# Host tests (no hardware needed)
CC     ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -I../app -I../hr

test: test_steps test_display test_time
	./test_steps && ./test_display && ./test_time && $(MAKE) -C ../hr test

test_steps: test_steps.c ../app/stepcount.c
	$(CC) $(CFLAGS) -o $@ $^ -lm
test_display: test_display.c ../app/display.c
	$(CC) $(CFLAGS) -o $@ $^
test_time: test_time.c ../app/timeutil.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_steps test_display test_time
	$(MAKE) -C ../hr clean
