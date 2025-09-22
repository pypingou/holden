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

# Installation directories
PREFIX ?= /usr/local
BINDIR_INSTALL = $(DESTDIR)$(PREFIX)/bin
SBINDIR_INSTALL = $(DESTDIR)$(PREFIX)/sbin
INCLUDEDIR_INSTALL = $(DESTDIR)$(PREFIX)/include/holden
SYSCONFDIR_INSTALL = $(DESTDIR)/etc/holden
UNITDIR_INSTALL = $(DESTDIR)/usr/lib/systemd/system
DOCDIR_INSTALL = $(DESTDIR)$(PREFIX)/share/doc/holden
MANDIR_INSTALL = $(DESTDIR)$(PREFIX)/share/man

install: all
	# Create directories
	install -d $(BINDIR_INSTALL)
	install -d $(SBINDIR_INSTALL)
	install -d $(INCLUDEDIR_INSTALL)
	install -d $(SYSCONFDIR_INSTALL)
	install -d $(DOCDIR_INSTALL)
	install -d $(MANDIR_INSTALL)/man1
	install -d $(MANDIR_INSTALL)/man8

	# Install binaries
	install -m 755 $(BINDIR)/controller $(BINDIR_INSTALL)/holden-controller
	install -m 755 $(BINDIR)/monitor $(BINDIR_INSTALL)/holden-monitor
	install -m 755 $(BINDIR)/agent $(SBINDIR_INSTALL)/holden-agent

	# Install headers
	install -m 644 protocol.h $(INCLUDEDIR_INSTALL)/
	install -m 644 cgroups.h $(INCLUDEDIR_INSTALL)/

	# Install documentation
	install -m 644 README.md $(DOCDIR_INSTALL)/
	install -m 644 TESTING.md $(DOCDIR_INSTALL)/
	install -m 644 LICENSE $(DOCDIR_INSTALL)/

	# Install configuration files
	install -m 644 config/agent.conf $(SYSCONFDIR_INSTALL)/
	install -m 755 config/holden-agent-wrapper $(SBINDIR_INSTALL)/

	# Install manual pages (if they exist)
	@if [ -f holden-controller.1 ]; then \
		install -m 644 holden-controller.1 $(MANDIR_INSTALL)/man1/; \
	fi
	@if [ -f holden-agent.8 ]; then \
		install -m 644 holden-agent.8 $(MANDIR_INSTALL)/man8/; \
	fi

install-systemd: install
	# Install systemd service (requires UNITDIR_INSTALL)
	install -d $(UNITDIR_INSTALL)
	install -m 644 holden-agent.service $(UNITDIR_INSTALL)/

rpm: all
	@echo "Building RPM packages..."
	./build-rpm.sh

srpm:
	@echo "Building source RPM package..."
	rpmdev-setuptree
	tar --transform 's,^,holden-1.0.0/,' \
		--exclude='bin/' --exclude='obj/' --exclude='.git/' \
		--exclude='*.rpm' --exclude='*.tar.gz' \
		-czf ~/rpmbuild/SOURCES/holden-1.0.0.tar.gz *
	cp holden.spec ~/rpmbuild/SPECS/
	rpmbuild -bs ~/rpmbuild/SPECS/holden.spec

help:
	@echo "Holden Process Orchestration System"
	@echo "==================================="
	@echo "Named after 19th century puppeteer Joseph Holden"
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
	@echo "  holden       - Controller and monitor"
	@echo "  holden-agent - Agent daemon with systemd"
	@echo "  holden-devel - Development headers"
	@echo
	@echo "Usage after build:"
	@echo "  ./bin/agent                          # Start agent"
	@echo "  ./bin/controller start <cmd> [args]  # Start process"
	@echo "  ./bin/controller list                # List processes"
	@echo "  ./bin/controller stop <pid>          # Stop process"
	@echo "  ./bin/controller constrain <pid> <mem_mb> <cpu_%>"
	@echo "  ./bin/monitor                        # Monitor processes"
	@echo
	@echo "After RPM install:"
	@echo "  sudo systemctl start holden-agent   # Start agent service"
	@echo "  holden-controller start sleep 30    # Start process"
	@echo "  holden-monitor                       # Monitor processes"