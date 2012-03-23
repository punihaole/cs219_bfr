CC = gcc
CFLAGS = -g -O2 -Wall -iquote ../include -iquote src/include
CLINK = -L../lib -lcon -lccnu -pthread -lm -lrt
SRC = src
TEST_SRC = src/tests
OBJ = bin/obj

all: $(OBJ) $(OBJ)/cluster.o $(OBJ)/bfr.o $(OBJ)/bfr_listener.o $(OBJ)/bfr_net_listener.o $(OBJ)/bfrd.o $(OBJ)/distance_table.o $(OBJ)/grid.o $(OBJ)/bfrd_main.o $(OBJ)/strategy.o $(OBJ)/bfr_test.o bfrd libbfr.a

$(OBJ):
	mkdir -p $(OBJ)

libbfr.a: $(OBJ) $(OBJ)/bfr.o
	ar rc libbfr.a $(OBJ)/bfr.o ../lib/libcon.a
	ranlib libbfr.a

bfrd: $(OBJ) $(OBJ)/cluster.o $(OBJ)/bfr.o $(OBJ)/bfr_listener.o $(OBJ)/bfr_net_listener.o $(OBJ)/bfrd.o $(OBJ)/distance_table.o $(OBJ)/grid.o $(OBJ)/bfrd_main.o $(OBJ)/strategy.o $(OBJ)/bfr_test.o
	$(CC) $(CFLAGS) $(OBJ)/*.o $(CLINK) -o bfrd
	
clean:
	rm -f $(SRC)/*~
	rm -rf $(OBJ)
	rm -f bfrd
	rm -f libbfr.a

$(OBJ)/bfr.o: $(SRC)/bfr.c
	$(CC) $(CFLAGS) -c $(SRC)/bfr.c -o $(OBJ)/bfr.o

$(OBJ)/bfr_listener.o: $(SRC)/bfr_listener.c $(OBJ)/grid.o $(OBJ)/bfr.o
	$(CC) $(CFLAGS) -c $(SRC)/bfr_listener.c -o $(OBJ)/bfr_listener.o

$(OBJ)/bfr_net_listener.o: $(SRC)/bfr_net_listener.c $(OBJ)/bfr.o
	$(CC) $(CFLAGS) -c $(SRC)/bfr_net_listener.c -o $(OBJ)/bfr_net_listener.o

$(OBJ)/bfrd.o: $(SRC)/bfrd.c $(OBJ)/bfr.o $(OBJ)/grid.o $(OBJ)/bfr_listener.o $(OBJ)/bfr_net_listener.o $(OBJ)/strategy.o
	$(CC) $(CFLAGS) -c $(SRC)/bfrd.c -o $(OBJ)/bfrd.o

$(OBJ)/grid.o: $(SRC)/grid.c
	$(CC) $(CFLAGS) -c $(SRC)/grid.c -o $(OBJ)/grid.o

$(OBJ)/bfrd_main.o: $(SRC)/bfrd_main.c $(OBJ)/bfr.o $(OBJ)/bfrd.o $(OBJ)/bfr_listener.o $(OBJ)/bfr_net_listener.o $(OBJ)/bfr_test.o $(OBJ)/strategy.o
	$(CC) $(CFLAGS) -c $(SRC)/bfrd_main.c -o $(OBJ)/bfrd_main.o

$(OBJ)/strategy.o: $(SRC)/strategy.c $(OBJ)/bfr.o
	$(CC) $(CFLAGS) -c $(SRC)/strategy.c -o $(OBJ)/strategy.o

$(OBJ)/bfr_test.o: $(TEST_SRC)/bfr_test.c $(OBJ)/bfr.o
	$(CC) $(CFLAGS) -c $(TEST_SRC)/bfr_test.c -o $(OBJ)/bfr_test.o

$(OBJ)/cluster.o: $(SRC)/cluster.c $(OBJ)/bfrd.o
	$(CC) $(CFLAGS) -c $(SRC)/cluster.c -o $(OBJ)/cluster.o

$(OBJ)/distance_table.o: $(SRC)/distance_table.c
	$(CC) $(CFLAGS) -c $(SRC)/distance_table.c -o $(OBJ)/distance_table.o
