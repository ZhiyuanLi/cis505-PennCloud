TARGETS = frontend

all: $(TARGETS)

frontend: frontend.cc request.cc response.cc helper.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
