# Target exe file
TARGET = detection.exe

# compilator
CC = gcc

# Flags
CFLAGS = -Wall -g

# link library
LIBS = -lcjson

# sources C
SRC = detect.c

# default rule
all: $(TARGET)

# How to create target exe file (TARGET) from source file
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)
	#make it executable
	chmod +x $(TARGET)
	
# remove target exe file
clean:
	rm -f $(TARGET)
