TARGETS = master

all: $(TARGETS)

master: master.cc conhash.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
