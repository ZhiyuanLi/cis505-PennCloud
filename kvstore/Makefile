TARGETS = kvstore

all: $(TARGETS)

kvstore: kvstore.cc chunkserver.cc lrucache.cc node.cc tools.cc
	g++ $^ -pthread -o $@

clean::
	rm -fv $(TARGETS) *~ *.o