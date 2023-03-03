CC = $(CROSS_COMPILE)gcc

#Flags
CFLAGS = -Wall -Werror -g

LDFLAG = -pthread -lrt 

#Source
SRCS = aesdsocket.c

#Object
OBJS = $(SRCS:.c=.o)

# Target
TARGET = aesdsocket

all: $(TARGET)
default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAG) $^ -o $@


.PHONY: clean

clean:
	$(RM) $(TARGET) *.o
