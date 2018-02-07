all:	server client
server:	tcpserver.cpp
	g++ tcpserver.cpp -o server -lcrypto

client:	tcpclient.cpp
	g++ tcpclient.cpp -o client -lcrypto
clean:
	-rm server client
