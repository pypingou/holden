CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS =

SRCDIR = .
OBJDIR = obj
BINDIR = bin

SOURCES = protocol.c cgroups.c
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

TARGETS = $(BINDIR)/agent $(BINDIR)/controller $(BINDIR)/monitor

.PHONY: all clean install

all: $(TARGETS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/agent: $(SRCDIR)/agent.c $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ $(LDFLAGS)

$(BINDIR)/controller: $(SRCDIR)/controller.c $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ $(LDFLAGS)

$(BINDIR)/monitor: $(SRCDIR)/monitor.c $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: all
	install -d /usr/local/bin
	install -m 755 $(BINDIR)/agent /usr/local/bin/orchestrator-agent
	install -m 755 $(BINDIR)/controller /usr/local/bin/orchestrator-controller
	install -m 755 $(BINDIR)/monitor /usr/local/bin/orchestrator-monitor

help:
	@echo "Process Orchestration System"
	@echo "============================"
	@echo
	@echo "Available targets:"
	@echo "  all       - Build all components"
	@echo "  clean     - Clean build artifacts"
	@echo "  install   - Install binaries to /usr/local/bin"
	@echo "  help      - Show this help message"
	@echo
	@echo "Components:"
	@echo "  agent     - Process agent (runs in container)"
	@echo "  controller- Main controller"
	@echo "  monitor   - Process monitor utility"
	@echo
	@echo "Usage after build:"
	@echo "  ./bin/agent                          # Start agent"
	@echo "  ./bin/controller start <cmd> [args]  # Start process"
	@echo "  ./bin/controller list                # List processes"
	@echo "  ./bin/controller stop <pid>          # Stop process"
	@echo "  ./bin/controller constrain <pid> <mem_mb> <cpu_%>"
	@echo "  ./bin/monitor                        # Monitor processes"