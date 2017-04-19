TARGETS = frontend

all: $(TARGETS)

frontend: frontend.cc request.cc response.cc helper.cc store.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
