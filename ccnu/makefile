CC = gcc
CFLAGS = -g -O2 -Wall -iquote ../include -iquote src/include
CLINK = -L../lib -lcon -lbfr -pthread -lm -lrt
SRC = src
TEST_SRC = src/tests
OBJ = bin/obj

all: $(OBJ) $(OBJ)/ccnu.o $(OBJ)/ccnud.o $(OBJ)/ccnu_net_test.o $(OBJ)/ccnud_listener.o $(OBJ)/ccnud_net_broadcaster.o $(OBJ)/ccnud_net_listener.o $(OBJ)/ccnud_cs.o $(OBJ)/ccnud_pit.o $(OBJ)/ccnud_stats.o libccnu.a ccnud

ccnud: bin/obj $(OBJ)/ccnud.o
	$(CC) $(CFLAGS) $(OBJ)/*.o $(CLINK) -o ccnud
	
clean:
	rm -f $(SRC)/*~
	rm -rf $(OBJ)
	rm -f ccnud
	rm -f libccnu.a
	rm -f src/tests/libccnu.a

$(OBJ):
	mkdir -p $(OBJ)

libccnu.a: $(OBJ) $(OBJ)/ccnu.o
	ar rc libccnu.a $(OBJ)/ccnu.o ../lib/libcon.a
	ranlib libccnu.a
	cp -p libccnu.a src/tests

$(OBJ)/ccnu.o: $(SRC)/ccnu.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnu.c -o $(OBJ)/ccnu.o

$(OBJ)/ccnud.o: $(SRC)/ccnud.c $(OBJ)/ccnud_listener.o $(OBJ)/ccnud_net_broadcaster.o $(OBJ)/ccnud_net_listener.o $(OBJ)/ccnud_cs.o $(OBJ)/ccnu_net_test.o $(OBJ)/ccnud_stats.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud.c -o $(OBJ)/ccnud.o

$(OBJ)/ccnud_listener.o: $(SRC)/ccnud_listener.c $(OBJ)/ccnud_cs.o $(OBJ)/ccnud_net_broadcaster.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_listener.c -o $(OBJ)/ccnud_listener.o

$(OBJ)/ccnud_net_broadcaster.o: $(SRC)/ccnud_net_broadcaster.c $(OBJ)/ccnud_pit.o $(OBJ)/ccnud_stats.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_net_broadcaster.c -o $(OBJ)/ccnud_net_broadcaster.o

$(OBJ)/ccnud_net_listener.o: $(SRC)/ccnud_net_listener.c $(OBJ)/ccnud_pit.o $(OBJ)/ccnu.o $(OBJ)/ccnud_net_broadcaster.o $(OBJ)/ccnud_cs.o $(OBJ)/ccnud_pit.o $(OBJ)/ccnud_stats.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_net_listener.c -o $(OBJ)/ccnud_net_listener.o

$(OBJ)/ccnud_cs.o: $(SRC)/ccnud_cs.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_cs.c -o $(OBJ)/ccnud_cs.o

$(OBJ)/ccnud_pit.o: $(SRC)/ccnud_pit.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_pit.c -o $(OBJ)/ccnud_pit.o

$(OBJ)/ccnud_stats.o: $(SRC)/ccnud_stats.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_stats.c -o $(OBJ)/ccnud_stats.o

$(OBJ)/ccnu_net_test.o: $(TEST_SRC)/ccnu_net_test.c $(OBJ)/ccnu.o
		$(CC) $(CFLAGS) -c $(TEST_SRC)/ccnu_net_test.c -o $(OBJ)/ccnu_net_test.o
