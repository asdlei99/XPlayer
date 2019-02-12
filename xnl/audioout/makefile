.SUFFIXES:.c .o

CC=g++

CPPFLAGS=-lstdc++
SRCS=codec.cpp

OBJS=$(SRCS:.cpp=.o)

EXEC=../../x64/debug/codec.so

start: $(OBJS)
	$(CC) $(CPPFLAGS) -shared -o $(EXEC) $(OBJS) -fPIC -lasound -Wno-strict-aliasing -Wno-sign-compare -Wno-extra -fno-stack-protector -fvisibility=hidden -Wl,-z,defs

.cpp.o:
	$(CC) -fPIC -Wall -o $@ -c $<

clean:
	rm -rf $(EXEC) $(OBJS)
