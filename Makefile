COBJECTS = aaediclock.o  
PKGCONFIG_LIBS = `pkg-config --libs sdl3` `pkg-config --libs sdl3-ttf`  `pkg-config --libs zlib` '-lm'
PKGCONFIG_CFLAGS = `pkg-config --cflags sdl3`  `pkg-config --cflags sdl3-ttf` `pkg-config --cflags zlib`

all: aaediclock

aaediclock : $(COBJECTS) 
	gcc -o clock $(COBJECTS)  $(PKGCONFIG_LIBS) $(CFLAGS)
	
$(COBJECTS): %.o: %.cc %.h
	gcc $< -c  $(PKGCONFIG_CFLAGS) -o $@ -Os

	
clean:
	-rm -f *.o
	-rm -f clock
	-rm *~

test:

	echo $(PKGCONFIG_LIBS)
	echo $(PKGCONFIG_CFLAGS)
