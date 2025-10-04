
#!/bin/bash

# HTTP Tests Runner Script for ur-mavrouter
# This script builds the router and runs all HTTP tests

set -e

echo "========================================="
echo "ur-mavrouter HTTP Server Test Suite"
echo "========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: Python 3 is required but not installed${NC}"
    exit 1
fi

# Install requirements
echo -e "${YELLOW}Installing test requirements...${NC}"
pip3 install -r requirements.txt --quiet

# Build the router with HTTP support
echo -e "${YELLOW}Building ur-mavrouter with HTTP support...${NC}"
cd ../pkg_src
make clean > /dev/null 2>&1 || true
cd build
cmake -D_BUILD_HTTP=ON .. || {
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
}
make -j || {
    echo -e "${RED}Build failed${NC}"
    exit 1
}
cd ../../http-tests

echo -e "${GREEN}Build successful${NC}"
echo ""

# Run the tests
echo -e "${YELLOW}Running HTTP server tests...${NC}"
echo "========================================="

# Run main test suite
python3 test_http_server.py || {
    echo -e "${RED}HTTP server tests failed${NC}"
    exit 1
}

echo ""
echo -e "${YELLOW}Running RPC controller tests...${NC}"
echo "========================================="

# Run RPC controller tests  
python3 test_rpc_controller.py || {
    echo -e "${RED}RPC controller tests failed${NC}"
    exit 1
}

echo ""
echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}All tests passed successfully!${NC}"
echo -e "${GREEN}=========================================${NC}"
