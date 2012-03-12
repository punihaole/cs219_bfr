CC = gcc
CFLAGS = -O2 -Wall -iquote ../include -iquote src/include
CLINK = -L../lib -lcon -lccnumr -pthread -lm 
SRC = src
TEST_SRC = src/tests
OBJ = bin/obj

all: output_dir ccnu.o ccnud.o ccnu_net_test.o ccnud_listener.o ccnud_net_broadcaster.o ccnud_net_listener.o ccnud_cs.o ccnud_pit.o lib daemon

daemon: output_dir ccnud.o
	$(CC) $(CLAGS) $(OBJ)/*.o $(CLINK) -o ccnud
	
clean:
	rm -f $(SRC)/*~
	rm -rf $(OBJ)

output_dir:
	mkdir -p $(OBJ)

lib: output_dir ccnu.o
	ar rc libccnu.a $(OBJ)/ccnu.o ../lib/libcon.a
	ranlib libccnu.a
	cp -p libccnu.a src/tests/libccnu.a

ccnu.o: $(SRC)/ccnu.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnu.c -o $(OBJ)/ccnu.o

ccnud.o: $(SRC)/ccnud.c ccnud_listener.o ccnud_net_broadcaster.o ccnud_net_listener.o ccnud_cs.o ccnu_net_test.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud.c -o $(OBJ)/ccnud.o

ccnud_listener.o: $(SRC)/ccnud_listener.c ccnud_cs.o ccnud_net_broadcaster.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_listener.c -o $(OBJ)/ccnud_listener.o

ccnud_net_broadcaster.o: $(SRC)/ccnud_net_broadcaster.c ccnud_net_listener.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_net_broadcaster.c -o $(OBJ)/ccnud_net_broadcaster.o

ccnud_net_listener.o: $(SRC)/ccnud_net_listener.c ccnud_pit.o ccnu.o ccnud_net_broadcaster.o ccnud_cs.o ccnud_pit.o
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_net_listener.c -o $(OBJ)/ccnud_net_listener.o

ccnud_cs.o: $(SRC)/ccnud_cs.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_cs.c -o $(OBJ)/ccnud_cs.o

ccnud_pit.o: $(SRC)/ccnud_pit.c
	$(CC) $(CFLAGS) -c $(SRC)/ccnud_pit.c -o $(OBJ)/ccnud_pit.o

ccnu_net_test.o: $(TEST_SRC)/ccnu_net_test.c ccnu.o
		$(CC) $(CFLAGS) -c $(TEST_SRC)/ccnu_net_test.c -o $(OBJ)/ccnu_net_test.o
