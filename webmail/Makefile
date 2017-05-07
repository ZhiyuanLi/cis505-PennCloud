TARGETS = webmail_client webmail_server webmail_deliver draft

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

webmail_client: webmail_client.cc client_def.o
	g++ $^ -lpthread -g -o $@

webmail_server: webmail_server.cc server_def.o
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@
	
webmail_deliver: webmail_deliver.cc server_def.o
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@
	
clean::
	rm -fv $(TARGETS) *.o *~
