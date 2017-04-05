TARGETS = frontend

all: $(TARGETS)

frontend: frontend.cc threadpool.cc debug.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
