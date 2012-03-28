LIB_DIR = lib
BIN_DIR = bin

all: $(LIB_DIR) $(LIB_BIN) libcon.a libbfr.a libccnu.a libccnf.a bfrd ccnud ccnfd apps

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	cd libcon && echo "cleaning libcon\n" && make clean;\
	cd ../bfr && echo "cleaning bfr\n" && make clean; \
	cd ../ccnu && echo "cleaning ccnu\n" && make clean; \
	cd ../ccnflood && echo "cleaning ccnflood\n" && make clean; \
	cd ../apps && echo "cleaning apps\n" && make clean; \
	cd ../$(BIN_DIR) && rm -f *; \
	cd ../$(LIB_DIR) && rm -f *; \
	rm -f *~
	rm -f include/*~
	rm -f *~
	rm -f scripts/*~	

libcon.a:
	cd libcon; echo "compiling the shared content lib"; \
	make; \
	cp libcon.a ../lib;

libbfr.a: $(LIB_DIR)/libcon.a
	cd bfr; echo "compiling libbfr.a and copying to lib"; \
	make libbfr.a; \
	cp libbfr.a ../lib/libbfr.a;
	
libccnu.a: $(LIB_DIR)/libcon.a
	cd ccnu; echo "compiling libccnu.a and copying to lib"; \
	make libccnu.a; \
	cp libccnu.a ../lib/libccnu.a;

libccnf.a: $(LIB_DIR)/libcon.a
	cd ccnflood; echo "compiling libccnf.a and copying to lib"; \
	make libccnf.a; \
	cp libccnf.a ../lib/libccnf.a;

bfrd: $(LIB_DIR)/libcon.a $(LIB_DIR)/libccnu.a
	cd bfr; echo "compiling bfrd and copying to bin"; \
	make bfrd; \
	cp bfrd ../bin/bfrd;

ccnud: $(LIB_DIR)/libcon.a $(LIB_DIR)/libbfr.a
	cd ccnu; echo "compiling ccnud and copying to bin"; \
	make ccnud; \
	cp ccnud ../bin/ccnud;

ccnfd: $(LIB_DIR)/libcon.a
	cd ccnflood; echo "compiling ccnfd and copying to bin"; \
	make ccnfd; \
	cp ccnfd ../bin/ccnfd;

apps: $(LIB_DIR)/libcon.a $(LIB_DIR)/libbfr.a $(LIB_DIR)/libccnu.a $(LIB_DIR)/libccnf.a
	cd apps; echo "compiling apps"; \
	make;
