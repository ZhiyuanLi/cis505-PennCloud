TARGETS = master

all: $(TARGETS)

master: master.cc
	g++ $^ -lpthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
