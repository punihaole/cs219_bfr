CC = gcc
CFLAGS = -O2 -Wall -iquote ../include
CLINK = -L../lib -lccnu -lccnf -lbfr -lcon -pthread -lm -lrt
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

all: $(BIN_DIR) $(OBJ_DIR) $(BIN_DIR)/gchunk $(BIN_DIR)/pchunk $(BIN_DIR)/getsummary $(BIN_DIR)/cftp $(BIN_DIR)/cftps $(BIN_DIR)/cftpr $(BIN_DIR)/cloc $(BIN_DIR)/cstp $(BIN_DIR)/trackroute

clean:
	rm -f $(SRC_DIR)/*~
	rm -rf $(OBJ_DIR)
	rm -rf $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR)/gchunk: $(OBJ_DIR)/gchunk.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/gchunk.o $(CLINK) -o $(BIN_DIR)/gchunk

$(BIN_DIR)/pchunk: $(OBJ_DIR)/pchunk.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/pchunk.o $(CLINK) -o $(BIN_DIR)/pchunk

$(BIN_DIR)/getsummary: $(OBJ_DIR)/getsummary.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/getsummary.o $(CLINK) -o $(BIN_DIR)/getsummary

$(BIN_DIR)/cftp: $(OBJ_DIR)/cftp.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cftp.o $(CLINK) -o $(BIN_DIR)/cftp

$(BIN_DIR)/cftps: $(OBJ_DIR)/cftps.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cftps.o $(CLINK) -o $(BIN_DIR)/cftps

$(BIN_DIR)/cftpr: $(OBJ_DIR)/cftpr.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cftpr.o $(CLINK) -o $(BIN_DIR)/cftpr

$(BIN_DIR)/cloc: $(OBJ_DIR)/cloc.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cloc.o $(CLINK) -o $(BIN_DIR)/cloc

$(BIN_DIR)/cstp: $(OBJ_DIR)/cstp.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/cstp.o $(CLINK) -o $(BIN_DIR)/cstp

$(BIN_DIR)/trackroute: $(OBJ_DIR)/trackroute.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/trackroute.o $(CLINK) -o $(BIN_DIR)/trackroute

$(OBJ_DIR)/gchunk.o: $(SRC_DIR)/gchunk.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/gchunk.c -o $(OBJ_DIR)/gchunk.o

$(OBJ_DIR)/pchunk.o: $(SRC_DIR)/pchunk.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/pchunk.c -o $(OBJ_DIR)/pchunk.o

$(OBJ_DIR)/getsummary.o: $(SRC_DIR)/getsummary.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/getsummary.c -o $(OBJ_DIR)/getsummary.o	

$(OBJ_DIR)/cftp.o: $(SRC_DIR)/cftp.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cftp.c -o $(OBJ_DIR)/cftp.o

$(OBJ_DIR)/cftps.o: $(SRC_DIR)/cftps.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cftps.c -o $(OBJ_DIR)/cftps.o

$(OBJ_DIR)/cftpr.o: $(SRC_DIR)/cftpr.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cftpr.c -o $(OBJ_DIR)/cftpr.o

$(OBJ_DIR)/cloc.o: $(SRC_DIR)/cloc.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cloc.c -o $(OBJ_DIR)/cloc.o

$(OBJ_DIR)/cstp.o: $(SRC_DIR)/cstp.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/cstp.c -o $(OBJ_DIR)/cstp.o

$(OBJ_DIR)/trackroute.o: $(SRC_DIR)/trackroute.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/trackroute.c -o $(OBJ_DIR)/trackroute.o
