SOURCE := ed.c argparse.c
TARGET := ed

all: $(TARGET)

$(TARGET): $(SOURCE)
	cc -fsanitize=address -Wall -Wextra -Wformat=2 -Wimplicit-fallthrough \
		-Wshadow -Wpointer-arith -Wswitch-enum -Wconversion -Wparentheses \
		-Werror $(SOURCE) -o $(TARGET) -g

clean:
	rm -f $(TARGET)
