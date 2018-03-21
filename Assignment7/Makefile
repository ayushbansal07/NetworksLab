all:	libctp.a client server

libctp.a:	CTP.h CTP.cpp
	g++ CTP.cpp -c
	ar rc libctp.a CTP.o
	rm CTP.o
client:	CTP.cpp CTP.h ctpclient.cpp
	g++ CTP.cpp ctpclient.cpp libctp.a -lpthread -o client
server:	CTP.cpp CTP.h ctpserver.cpp
	g++ CTP.cpp ctpserver.cpp libctp.a -lpthread -o server
clean:
	rm client server libctp.a
