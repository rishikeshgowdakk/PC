CC = gcc
CFLAGS = -O2 -Wall -Wextra -fopenmp
TARGET = pixelsprint
SOURCE = src/pixelsprint.c

.PHONY: all run benchmark clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) --demo

benchmark: $(TARGET)
	./$(TARGET) --benchmark

clean:
	rm -f $(TARGET)
	rm -f output/before.bmp output/after.bmp output/results.csv
	@rmdir output 2>/dev/null || true
