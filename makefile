LIB_DIR = lib
BIN_DIR = bin

all: output_dirs $(LIB_DIR)/libcon.a $(LIB_DIR)/libccnumr.a $(LIB_DIR)/libccnu.a $(BIN_DIR)/ccnumrd $(BIN_DIR)/ccnud apps

output_dirs:
	mkdir -p $(LIB_DIR)
	mkdir -p $(BIN_DIR)

clean:
	cd libcon && echo "cleaning libcon\n" && make clean;\
	cd ../ccnumr && echo "cleaning ccnumr\n" && make clean; \
	cd ../ccnu && echo "cleaning ccnu\n" && make clean; \
	cd ../apps && echo "cleaning apps\n" && make clean; \
	cd ../$(BIN_DIR) && rm -f *; \
	cd ../$(LIB_DIR) && rm -f *;

$(LIB_DIR)/libcon.a:
	cd libcon; echo "compiling the shared content lib"; \
	make; \
	cp libcon.a ../lib;

$(LIB_DIR)/libccnumr.a: $(LIB_DIR)/libcon.a
	cd ccnumr; echo "compiling libccnumr.a and copying to lib"; \
	make lib; \
	cp libccnumr.a ../lib/libccnumr.a;
	
$(LIB_DIR)/libccnu.a: $(LIB_DIR)/libcon.a
	cd ccnu; echo "compiling libccnu.a and copying to lib"; \
	make lib; \
	cp libccnu.a ../lib/libccnu.a;

$(BIN_DIR)/ccnumrd: $(LIB_DIR)/libcon.a $(LIB_DIR)/libccnu.a
	cd ccnumr; echo "compiling ccnumrd and copying to bin"; \
	make daemon; \
	cp ccnumrd ../bin/ccnumrd;

$(BIN_DIR)/ccnud: $(LIB_DIR)/libcon.a $(LIB_DIR)/libccnumr.a
	cd ccnu; echo "compiling ccnud and copying to bin"; \
	make daemon; \
	cp ccnud ../bin/ccnud;

apps: $(LIB_DIR)/libcon.a $(LIB_DIR)/libccnumr.a $(LIB_DIR)/libccnu.a
	cd apps; echo "compiling apps"; \
	make;
