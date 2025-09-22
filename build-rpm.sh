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

# Create source tarball using Makefile
echo -e "${BLUE}Creating source tarball...${NC}"
make dist

echo -e "${GREEN}✓ Source tarball created${NC}"

# Build RPM packages using standard RPM workflow
echo -e "${BLUE}Building RPM packages...${NC}"

# Build source and binary RPMs directly from tarball
echo -e "${YELLOW}Building SRPM and binary RPMs...${NC}"
rpmbuild -ta "$PACKAGE_NAME-$VERSION.tar.gz"

echo -e "${GREEN}✓ RPM build completed successfully!${NC}"
echo
echo -e "${BLUE}RPM packages have been built using your system's RPM configuration.${NC}"
echo "Check your RPM build directories (typically under ~/.rpmbuild/ or as configured in ~/.rpmmacros)"

echo
echo -e "${BLUE}Testing installation:${NC}"
echo "1. Locate packages in your RPM build directory"
echo "2. Install with: sudo dnf install <path-to-rpm>"
echo "3. Start agent: sudo systemctl start holden-agent"
echo "4. Test controller: holden-controller list"