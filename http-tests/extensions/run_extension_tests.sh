
#!/bin/bash

# Extension Management Tests Runner
# Runs all extension-related HTTP endpoint tests

echo "======================================"
echo "Extension Management Tests"
echo "======================================"
echo ""

# Check if server is running
echo "Checking if HTTP server is running on port 5000..."
if ! curl -s http://0.0.0.0:5000/status > /dev/null 2>&1; then
    echo "❌ ERROR: HTTP server is not running on port 5000"
    echo "Please start the server first with:"
    echo "  cd pkg_src && make && ./build/ur-mavrouter -j config/router-config.json -H config/http-server-config.json"
    exit 1
fi
echo "✅ Server is running"
echo ""

# Run tests
FAILED=0

echo "Running extension status tests..."
python3 http-tests/extensions/test_extension_status.py
if [ $? -ne 0 ]; then
    FAILED=$((FAILED + 1))
fi
echo ""

echo "Running extension add tests..."
python3 http-tests/extensions/test_extension_add.py
if [ $? -ne 0 ]; then
    FAILED=$((FAILED + 1))
fi
echo ""

echo "Running extension stop tests..."
python3 http-tests/extensions/test_extension_stop.py
if [ $? -ne 0 ]; then
    FAILED=$((FAILED + 1))
fi
echo ""

echo "Running extension start tests..."
python3 http-tests/extensions/test_extension_start.py
if [ $? -ne 0 ]; then
    FAILED=$((FAILED + 1))
fi
echo ""

echo "Running extension delete tests..."
python3 http-tests/extensions/test_extension_delete.py
if [ $? -ne 0 ]; then
    FAILED=$((FAILED + 1))
fi
echo ""

echo "Running comprehensive workflow test..."
python3 http-tests/extensions/test_extension_workflow.py
if [ $? -ne 0 ]; then
    FAILED=$((FAILED + 1))
fi
echo ""

# Summary
echo "======================================"
if [ $FAILED -eq 0 ]; then
    echo "✅ All extension tests passed!"
    exit 0
else
    echo "❌ $FAILED test(s) failed"
    exit 1
fi
