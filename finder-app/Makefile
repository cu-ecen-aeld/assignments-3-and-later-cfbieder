
ifndef CROSS_COMPILE
	CC = gcc
else
	CC = $(CROSS_COMPILE)gcc
endif


CFLAGS 

TARGET = writer

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET) $(TARGET).o
