CC = gcc
CFLAGS = -g -O2 -Wall -iquote ../include -iquote src/include
CLINK = -L../lib -lcon -pthread -lm -lrt
SRC = src
TEST_SRC = src/tests
OBJ = bin/obj

all: $(OBJ) $(OBJ)/ccnf.o $(OBJ)/ccnfd.o $(OBJ)/ccnf_net_test.o $(OBJ)/ccnfd_listener.o $(OBJ)/ccnfd_net_broadcaster.o $(OBJ)/ccnfd_net_listener.o $(OBJ)/ccnfd_cs.o $(OBJ)/ccnfd_pit.o $(OBJ)/ccnfd_stats.o libccnf.a ccnfd

ccnfd: bin/obj $(OBJ)/ccnfd.o
	$(CC) $(CFLAGS) $(OBJ)/*.o $(CLINK) -o ccnfd
	
clean:
	rm -f $(SRC)/*~
	rm -rf $(OBJ)
	rm -f ccnfd
	rm -f libccnf.a
	rm -f src/tests/libccnf.a

$(OBJ):
	mkdir -p $(OBJ)

libccnf.a: $(OBJ) $(OBJ)/ccnf.o
	ar rc libccnf.a $(OBJ)/ccnf.o ../lib/libcon.a
	ranlib libccnf.a
	cp -p libccnf.a src/tests

$(OBJ)/ccnf.o: $(SRC)/ccnf.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnf.c -o $(OBJ)/ccnf.o

$(OBJ)/ccnfd.o: $(SRC)/ccnfd.c $(OBJ)/ccnfd_listener.o $(OBJ)/ccnfd_net_broadcaster.o $(OBJ)/ccnfd_net_listener.o $(OBJ)/ccnfd_cs.o $(OBJ)/ccnf_net_test.o $(OBJ)/ccnfd_stats.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd.c -o $(OBJ)/ccnfd.o

$(OBJ)/ccnfd_listener.o: $(SRC)/ccnfd_listener.c $(OBJ)/ccnfd_cs.o $(OBJ)/ccnfd_net_broadcaster.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd_listener.c -o $(OBJ)/ccnfd_listener.o

$(OBJ)/ccnfd_net_broadcaster.o: $(SRC)/ccnfd_net_broadcaster.c $(OBJ)/ccnfd_pit.o $(OBJ)/ccnfd_stats.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd_net_broadcaster.c -o $(OBJ)/ccnfd_net_broadcaster.o

$(OBJ)/ccnfd_net_listener.o: $(SRC)/ccnfd_net_listener.c $(OBJ)/ccnfd_pit.o $(OBJ)/ccnf.o $(OBJ)/ccnfd_net_broadcaster.o $(OBJ)/ccnfd_cs.o $(OBJ)/ccnfd_pit.o $(OBJ)/ccnfd_stats.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd_net_listener.c -o $(OBJ)/ccnfd_net_listener.o

$(OBJ)/ccnfd_cs.o: $(SRC)/ccnfd_cs.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd_cs.c -o $(OBJ)/ccnfd_cs.o

$(OBJ)/ccnfd_pit.o: $(SRC)/ccnfd_pit.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd_pit.c -o $(OBJ)/ccnfd_pit.o

$(OBJ)/ccnfd_stats.o: $(SRC)/ccnfd_stats.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnfd_stats.c -o $(OBJ)/ccnfd_stats.o

$(OBJ)/ccnf_net_test.o: $(TEST_SRC)/ccnf_net_test.c $(OBJ)/ccnf.o
		$(CC) $(CFLAGS) -c $(TEST_SRC)/ccnf_net_test.c -o $(OBJ)/ccnf_net_test.o
