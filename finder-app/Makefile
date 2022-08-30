
#includes option for cross complile

ifndef CROSS_COMPILE
	CC = gcc
else
	CC = $(CROSS_COMPILE)gcc
endif

SRC := writer.c
TARGET = writer
OBJS := $(SRC:.c=.o)

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(TARGET).o
