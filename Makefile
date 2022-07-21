BUILDDIR := build

COMMON_SRCS := common/message.cpp tcp/tcplistener.cpp tcp/tcpstream.cpp
SERVER_SRCS := server/channel.cpp server/connection.cpp server/db.cpp server/main.cpp server/poll_registry.cpp
CLIENT_SRCS := client/client.cpp client/main.cpp

SERVER_DEPS := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SERVER_SRCS) $(COMMON_SRCS))
CLIENT_DEPS := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(CLIENT_SRCS) $(COMMON_SRCS))

INCLUDE_PATHS := common tcp
INCLUDE_FLAGS := $(patsubst %,-I%,$(INCLUDE_PATHS))

CPPFLAGS = -fsanitize=address -g -std=c++17 $(INCLUDE_FLAGS)

all: server client

run_server: server
	./$(BUILDDIR)/server/main

run_client: client
	./$(BUILDDIR)/client/main

server: $(BUILDDIR)/server/main

client: $(BUILDDIR)/client/main

.PHONY: run server client run_client run_server

echo:
	@echo $(CLIENT_DEPS)

$(BUILDDIR)/server/main: $(SERVER_DEPS)  | $(BUILDDIR)
	@printf "LINK\t$@\n"
	@g++ $(CPPFLAGS) $^ -o $@

$(BUILDDIR)/client/main: $(CLIENT_DEPS) | $(BUILDDIR)
	@printf "LINK\t$@\n"
	@g++ $(CPPFLAGS) $^ -o $@ -lpthread -lncurses

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	@printf "COMPILE\t$@\n"
	@g++ -c $(CPPFLAGS) $< -o $@

$(BUILDDIR):
	mkdir -p $@
	mkdir -p $@/client
	mkdir -p $@/server
	mkdir -p $@/common
	mkdir -p $@/tcp

clean:
	rm -rf $(BUILDDIR)

.PHONY: clean
