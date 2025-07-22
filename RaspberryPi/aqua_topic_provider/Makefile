# Target exe file
TARGET = provider

# compilator
CC = gcc

# Flags
CFLAGS = -Wall -g

# link library
LIBS = -lcjson -lmosquitto

# sources C
SRC = provider.c

# default rule
all: $(TARGET)

# How to create target exe file (TARGET) from source file
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)
	
# remove target exe file
clean:
	rm -f $(TARGET)
