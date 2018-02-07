all:	server client
server:	udpserver.cpp
	g++ udpserver.cpp -o server -lcrypto

client:	udpclient.cpp
	g++ udpclient.cpp -o client -lcrypto
clean:
	-rm server client
