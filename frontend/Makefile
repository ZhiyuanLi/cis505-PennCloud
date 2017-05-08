TARGETS = frontend lb

all: $(TARGETS)

frontend: frontend.cc request.cc response.cc utils.cc store.cc ../webmail/webmail_utils.cc
	g++ $^ -I/opt/local/include/ -L/opt/local/bin/openssl -lcrypto -lresolv -lpthread -o $@

lb: load_balancer.cc utils.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
