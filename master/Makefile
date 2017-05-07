TARGETS = master

all: $(TARGETS)

master: master.cc conhash.cc ../frontend/utils.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
