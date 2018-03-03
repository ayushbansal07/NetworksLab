all:	server client
server:	udpserver.cpp utils.h
	g++ udpserver.cpp -o server -lcrypto

client:	udpclient.cpp utils.h
	g++ udpclient.cpp -o client -lcrypto
clean:
	-rm server client
