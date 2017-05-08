TARGETS = webmail_client webmail_server draft3

all: $(TARGETS)

webmail_client: webmail_client.cc client_def.cc 
	g++ $^ -lpthread -g -o $@

webmail_server: webmail_server.cc webmail_utils.cc ../frontend/store.cc ../frontend/utils.cc
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@
	
# webmail_deliver: webmail_deliver.cc webmail_utils.cc ../frontend/store.cc ../frontend/utils.cc
#	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@
	
draft3: draft3.cc
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *.o *~
