TARGETS = frontend lb

all: $(TARGETS)

frontend: frontend.cc request.cc response.cc utils.cc store.cc
	g++ $^ -lpthread -o $@

lb: load_balancer.cc utils.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
