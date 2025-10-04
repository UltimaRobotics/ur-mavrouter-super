
#!/bin/bash

echo "========================================="
echo "ur-mavrouter HTTP Endpoint Test Suite"
echo "========================================="
echo ""

# Check if server is running
echo "Checking if HTTP server is accessible..."
if ! curl -s http://0.0.0.0:5000/status > /dev/null 2>&1; then
    echo "❌ ERROR: HTTP server is not running on port 5000"
    echo "Please start ur-mavrouter with HTTP server enabled first"
    exit 1
fi

echo "✅ Server is accessible"
echo ""

# Test files to run
test_files=(
    "test_root_endpoint.py"
    "test_status_endpoint.py"
    "test_threads_list_endpoint.py"
    "test_thread_status_endpoint.py"
    "test_thread_pause_endpoint.py"
    "test_thread_resume_endpoint.py"
    "test_thread_stop_endpoint.py"
    "test_error_handling.py"
    "test_performance.py"
)

failed_tests=0
passed_tests=0

for test_file in "${test_files[@]}"; do
    echo "========================================="
    echo "Running: $test_file"
    echo "========================================="
    
    if python3 "$test_file"; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    echo ""
done

echo "========================================="
echo "Test Summary"
echo "========================================="
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"
echo "Total: $((passed_tests + failed_tests))"
echo ""

if [ $failed_tests -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi
