CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

ifeq ($(shell uname -s),Darwin)
	CFLAGS += -I/opt/homebrew/include
	LIBS = -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif

SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/*.c)
TARGET = character_game

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)