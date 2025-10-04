
#!/bin/bash

echo "Checking port 5000 availability..."
echo "=================================="

# Check if port 5000 is in use
if command -v lsof &> /dev/null; then
    echo "Using lsof to check port 5000:"
    lsof -i :5000
    if [ $? -eq 0 ]; then
        echo ""
        echo "WARNING: Port 5000 is already in use!"
        echo "Please stop the process using this port or change the HTTP server port."
    else
        echo "Port 5000 is available."
    fi
elif command -v netstat &> /dev/null; then
    echo "Using netstat to check port 5000:"
    netstat -tuln | grep :5000
    if [ $? -eq 0 ]; then
        echo ""
        echo "WARNING: Port 5000 is already in use!"
    else
        echo "Port 5000 is available."
    fi
elif command -v ss &> /dev/null; then
    echo "Using ss to check port 5000:"
    ss -tuln | grep :5000
    if [ $? -eq 0 ]; then
        echo ""
        echo "WARNING: Port 5000 is already in use!"
    else
        echo "Port 5000 is available."
    fi
else
    echo "No suitable tool found to check port (lsof, netstat, or ss required)"
fi

echo ""
echo "Checking network configuration..."
echo "=================================="
ip addr show | grep -E "inet |inet6 " || ifconfig | grep -E "inet |inet6 "

echo ""
echo "To kill process on port 5000 (if needed):"
echo "  lsof -ti :5000 | xargs kill -9"
