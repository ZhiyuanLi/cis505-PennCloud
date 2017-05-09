TARGETS = webmail_server

all: $(TARGETS)

webmail_server: webmail_server.cc webmail_utils.cc ../frontend/store.cc ../frontend/utils.cc
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *.o *~
