CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS =

SRCDIR = .
OBJDIR = obj
BINDIR = bin

SOURCES = protocol.c cgroups.c
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

TARGETS = $(BINDIR)/agent $(BINDIR)/controller $(BINDIR)/monitor

.PHONY: all clean install rpm srpm

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

rpm: all
	@echo "Building RPM packages..."
	./build-rpm.sh

srpm:
	@echo "Building source RPM package..."
	rpmdev-setuptree
	tar --transform 's,^,process-orchestrator-1.0.0/,' \
		--exclude='bin/' --exclude='obj/' --exclude='.git/' \
		--exclude='*.rpm' --exclude='*.tar.gz' \
		-czf ~/rpmbuild/SOURCES/process-orchestrator-1.0.0.tar.gz *
	cp process-orchestrator.spec ~/rpmbuild/SPECS/
	rpmbuild -bs ~/rpmbuild/SPECS/process-orchestrator.spec

help:
	@echo "Process Orchestration System"
	@echo "============================"
	@echo
	@echo "Available targets:"
	@echo "  all       - Build all components"
	@echo "  clean     - Clean build artifacts"
	@echo "  install   - Install binaries to /usr/local/bin"
	@echo "  rpm       - Build RPM packages (requires rpmbuild)"
	@echo "  srpm      - Build source RPM package only"
	@echo "  help      - Show this help message"
	@echo
	@echo "Components:"
	@echo "  agent     - Process agent (runs in container)"
	@echo "  controller- Main controller"
	@echo "  monitor   - Process monitor utility"
	@echo
	@echo "RPM Packages:"
	@echo "  process-orchestrator       - Controller and monitor"
	@echo "  process-orchestrator-agent - Agent daemon with systemd"
	@echo "  process-orchestrator-devel - Development headers"
	@echo
	@echo "Usage after build:"
	@echo "  ./bin/agent                          # Start agent"
	@echo "  ./bin/controller start <cmd> [args]  # Start process"
	@echo "  ./bin/controller list                # List processes"
	@echo "  ./bin/controller stop <pid>          # Stop process"
	@echo "  ./bin/controller constrain <pid> <mem_mb> <cpu_%>"
	@echo "  ./bin/monitor                        # Monitor processes"