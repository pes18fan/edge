SOURCE := ed.c argparse.c
TARGET := ed

CC := cc
CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wformat=2 \
	-Wimplicit-fallthrough \
	-Wshadow \
	-Wpointer-arith \
	-Wswitch-enum \
	-Wconversion \
	-Wparentheses \
	-Werror
DEBUGFLAGS := -fsanitize=address -g

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(SOURCE) -o $(TARGET)

release: $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -O3 -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: clean release
