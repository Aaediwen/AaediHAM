COBJECTS = aaediclock.o utils.o modules.o classes.o
PKGCONFIG_LIBS = `pkg-config --libs sdl3` `pkg-config --libs sdl3-ttf`  `pkg-config --libs zlib` '-lm' '-lcurl' '-lsgp4s'
PKGCONFIG_CFLAGS = `pkg-config --cflags sdl3`  `pkg-config --cflags sdl3-ttf` `pkg-config --cflags zlib`
CFLAGS = -std=c++11 -Wall -Wextra -Werror

all: aaediclock

aaediclock : $(COBJECTS) 
	g++  -o clock $(COBJECTS)  $(PKGCONFIG_LIBS)  
#	gcc -o clock $(COBJECTS)  $(PKGCONFIG_LIBS) $(CFLAGS)
	
$(COBJECTS): %.o: %.cc %.h
	g++ $< -c  $(PKGCONFIG_CFLAGS) -o $@ -Os $(CFLAGS)
#	gcc $< -c  $(PKGCONFIG_CFLAGS) -o $@ -Os

	
clean:
	-rm -f *.o
	-rm -f clock
	-rm *~

test:

	echo $(PKGCONFIG_LIBS)
	echo $(PKGCONFIG_CFLAGS)
