

all: pbparser.cpp
	g++ pbparser.cpp  -g -o pbparser ./libprotobuf.a -I./include/
	
clean:
	rm pbparser 

