TARGETS = webmail_client webmail_server webmail_deliver draft3

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

webmail_client: webmail_client.cc client_def.o 
	g++ $^ -lpthread -g -o $@

webmail_server: webmail_server.cc webmail_utils.o ../frontend/store.cc ../frontend/utils.cc
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@
	
webmail_deliver: webmail_deliver.cc webmail_utils.o
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@
	
draft3: draft3.cc
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *.o *~
