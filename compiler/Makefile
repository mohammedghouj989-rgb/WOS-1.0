CC = clang
CFLAGS = -Wall -O2
TARGET = wos
SOURCES = wospras.c

all: $(TARGET)

$(TARGET): $(SOURCES) WOS.c codegen.c
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET) output.c output

run: $(TARGET)
	./$(TARGET) ../examples/hello.wos

.PHONY: all clean run
