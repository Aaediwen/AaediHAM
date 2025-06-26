COBJECTS = aaediclock.o utils.o modules.o
PKGCONFIG_LIBS = `pkg-config --libs sdl3` `pkg-config --libs sdl3-ttf`  `pkg-config --libs zlib` '-lm' '-lcurl'
PKGCONFIG_CFLAGS = `pkg-config --cflags sdl3`  `pkg-config --cflags sdl3-ttf` `pkg-config --cflags zlib`

all: aaediclock

aaediclock : $(COBJECTS) 
	g++ -o clock $(COBJECTS)  $(PKGCONFIG_LIBS) $(CFLAGS)
#	gcc -o clock $(COBJECTS)  $(PKGCONFIG_LIBS) $(CFLAGS)
	
$(COBJECTS): %.o: %.cc %.h
	g++ $< -c  $(PKGCONFIG_CFLAGS) -o $@ -Os
#	gcc $< -c  $(PKGCONFIG_CFLAGS) -o $@ -Os

	
clean:
	-rm -f *.o
	-rm -f clock
	-rm *~

test:

	echo $(PKGCONFIG_LIBS)
	echo $(PKGCONFIG_CFLAGS)
