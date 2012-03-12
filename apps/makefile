CC = gcc
CFLAGS = -g -O2 -Wall -iquote ../include
CLINK = -L../lib -lccnu -lcon -lccnumr -pthread -lm 
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

all: output_dir consumer_simple producer_simple getsummary cftp cftps

clean:
	rm -f $(SRC_DIR)/*~
	rm -f $(OBJ_DIR)/*
	rm -f $(BIN_DIR)/*

output_dir:
	mkdir -p bin
	mkdir -p obj

consumer_simple: consumer_simple.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/consumer_simple.o $(CLINK) -o $(BIN_DIR)/consumer_simple

producer_simple: producer_simple.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/producer_simple.o $(CLINK) -o $(BIN_DIR)/producer_simple

getsummary: getsummary.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/getsummary.o $(CLINK) -o $(BIN_DIR)/getsummary

cftp: cftp.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cftp.o $(CLINK) -o $(BIN_DIR)/cftp

cftps: cftps.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cftps.o $(CLINK) -o $(BIN_DIR)/cftps

consumer_simple.o: $(SRC_DIR)/consumer_simple.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/consumer_simple.c -o $(OBJ_DIR)/consumer_simple.o

producer_simple.o: $(SRC_DIR)/producer_simple.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/producer_simple.c -o $(OBJ_DIR)/producer_simple.o

getsummary.o: $(SRC_DIR)/getsummary.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/getsummary.c -o $(OBJ_DIR)/getsummary.o	

cftp.o: $(SRC_DIR)/cftp.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cftp.c -o $(OBJ_DIR)/cftp.o

cftps.o: $(SRC_DIR)/cftps.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cftps.c -o $(OBJ_DIR)/cftps.o
