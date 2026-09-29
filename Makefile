SOURCE := ed.c argparse.c
TARGET := ed

all: $(TARGET)

$(TARGET): $(SOURCE)
	cc -fsanitize=address -Wall -Wextra -Werror $(SOURCE) -o $(TARGET) -g

clean:
	rm -f $(TARGET)
