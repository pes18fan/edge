SOURCE := ed.c argparse.c
TARGET := ed

CC := cc
CFLAGS := \
	-Wall -Wextra -Wformat=2 -Wimplicit-fallthrough -Wshadow -Wpointer-arith \
	-Wswitch-enum -Wparentheses -Werror
DEBUGFLAGS := -fsanitize=address -g

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)
