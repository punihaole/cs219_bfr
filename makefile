all: libcon.a libccnumr.a libccnu.a ccnumrd ccnud apps

clean:
	cd libcon && echo "cleaning libcon\n" && make clean;\
	cd ../ccnumr && echo "cleaning ccnumr\n" && make clean; \
	cd ../ccnu && echo "cleaning ccnu\n" && make clean; \
	cd ../apps && echo "cleaning apps\n" && make clean; \
	cd ../bin && rm -f *; \
	cd ../lib && rm -f *;

libcon.a:
	cd libcon; echo "compiling the shared content lib"; \
	make; \
	cp libcon.a ../lib;

libccnumr.a: libcon.a
	cd ccnumr; echo "compiling libccnumr.a and copying to lib"; \
	make lib; \
	cp libccnumr.a ../lib/libccnumr.a;
	
libccnu.a: libcon.a
	cd ccnu; echo "compiling libccnu.a and copying to lib"; \
	make lib; \
	cp libccnu.a ../lib/libccnu.a;

ccnumrd: libcon.a libccnu.a
	cd ccnumr; echo "compiling ccnumrd and copying to bin"; \
	make daemon; \
	cp ccnumrd ../bin/ccnumrd;

ccnud: libcon.a libccnumr.a
	cd ccnu; echo "compiling ccnud and copying to bin"; \
	make daemon; \
	cp ccnud ../bin/ccnud;

apps: libcon.a libccnumr.a libccnu.a
	cd apps; echo "compiling apps"; \
	make;
