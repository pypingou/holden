#!/bin/bash

# RPM Build Script for Process Orchestrator
# Builds RPM packages for Fedora/RHEL/CentOS

set -e

# Configuration
PACKAGE_NAME="holden"
VERSION="0.1"
RELEASE="1"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check if running on RPM-based system
if ! command -v rpmbuild >/dev/null 2>&1; then
    echo -e "${RED}Error: rpmbuild not found. Install rpm-build package.${NC}"
    echo "Fedora/RHEL/CentOS: sudo dnf install rpm-build rpmdevtools"
    exit 1
fi

# Setup RPM build environment
echo -e "${BLUE}Setting up RPM build environment...${NC}"
mkdir -p "$HOME/rpmbuild"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create source tarball using Makefile
echo -e "${BLUE}Creating source tarball...${NC}"
make dist

# Copy tarball to rpmbuild SOURCES
cp "$PACKAGE_NAME-$VERSION.tar.gz" "$HOME/rpmbuild/SOURCES/"

echo -e "${GREEN}✓ Source tarball created${NC}"

# Copy spec file
cp holden.spec "$HOME/rpmbuild/SPECS/"

# Build RPM
echo -e "${BLUE}Building RPM packages...${NC}"
cd "$HOME/rpmbuild"

# Build source RPM
echo -e "${YELLOW}Building SRPM...${NC}"
rpmbuild -bs SPECS/holden.spec

# Build binary RPMs
echo -e "${YELLOW}Building binary RPMs...${NC}"
rpmbuild -bb SPECS/holden.spec

echo -e "${GREEN}✓ RPM build completed successfully!${NC}"
echo
echo -e "${BLUE}Generated packages:${NC}"
find "$HOME/rpmbuild/RPMS" -name "*.rpm" -exec basename {} \; | sort
find "$HOME/rpmbuild/SRPMS" -name "*.rpm" -exec basename {} \;

echo
echo -e "${BLUE}Package locations:${NC}"
echo "Binary RPMs: $HOME/rpmbuild/RPMS/"
echo "Source RPM:  $HOME/rpmbuild/SRPMS/"

echo
echo -e "${BLUE}Installation commands:${NC}"
echo "Main package:   sudo dnf install ~/rpmbuild/RPMS/x86_64/holden-$VERSION-$RELEASE.*.rpm"
echo "Agent package:  sudo dnf install ~/rpmbuild/RPMS/x86_64/holden-agent-$VERSION-$RELEASE.*.rpm"
echo "Devel package:  sudo dnf install ~/rpmbuild/RPMS/x86_64/holden-devel-$VERSION-$RELEASE.*.rpm"

echo
echo -e "${BLUE}Testing installation:${NC}"
echo "1. Install packages with dnf"
echo "2. Start agent: sudo systemctl start holden-agent"
echo "3. Test controller: holden-controller list"