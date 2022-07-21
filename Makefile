BUILDDIR := build

SERVER_DEPS := $(patsubst ./server/%.cpp,$(BUILDDIR)/server/%.o,$(wildcard ./server/*.cpp))
CLIENT_DEPS := $(BUILDDIR)/client/client.o $(BUILDDIR)/client/main.o $(BUILDDIR)/client/utils.o

CFLAGS = -fsanitize=address -g -std=c++17

all: server client

run_server: server
	./$(BUILDDIR)/server/main

run_client: client
	./$(BUILDDIR)/client/main

server: $(BUILDDIR)/server/main

client: $(BUILDDIR)/client/main

.PHONY: run server client run_client run_server



$(BUILDDIR)/server/main: $(SERVER_DEPS)  | $(BUILDDIR)
	g++ $(CFLAGS) $^ -o $@ -lpthread

$(BUILDDIR)/client/main: $(CLIENT_DEPS) | $(BUILDDIR)
	g++ $(CFLAGS) $^ -o $@ -lpthread -lncurses

$(BUILDDIR)/client/%.o: client/%.cpp client/*.hpp | $(BUILDDIR)
	g++ -c $(CFLAGS) $< -o $@

$(BUILDDIR)/server/%.o: server/%.cpp server/*.hpp | $(BUILDDIR)
	g++ -c $(CFLAGS) $< -o $@



$(BUILDDIR):
	mkdir -p $@
	mkdir -p $@/client
	mkdir -p $@/server

clean:
	rm -rf $(BUILDDIR)

.PHONY: clean
