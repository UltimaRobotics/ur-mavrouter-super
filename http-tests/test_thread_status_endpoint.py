#!/usr/bin/env python3
"""
HTTP test for individual thread status endpoint (GET /api/threads/{name})
"""

import requests
import sys
import json

BASE_URL = "http://0.0.0.0:5000"

def test_thread_status(thread_name):
    """Test GET /api/threads/{name} - Get specific thread status"""
    print(f"Testing thread status endpoint for '{thread_name}'...")

    try:
        response = requests.get(f"{BASE_URL}/api/threads/{thread_name}", timeout=5)

        # Check status code
        if response.status_code != 200:
            print(f"❌ FAILED: Expected status 200, got {response.status_code}")
            return False

        # Check content type
        if "application/json" not in response.headers.get("Content-Type", ""):
            print(f"❌ FAILED: Expected application/json content type")
            return False

        # Parse JSON
        try:
            data = response.json()
        except json.JSONDecodeError:
            print(f"❌ FAILED: Invalid JSON response")
            return False

        # Check response structure
        if "threads" not in data:
            print(f"❌ FAILED: Missing 'threads' field in response")
            return False

        if thread_name not in data["threads"]:
            print(f"❌ FAILED: Thread '{thread_name}' not found in response")
            return False

        # Verify thread info
        thread_info = data["threads"][thread_name]
        required_fields = ["threadId", "state", "isAlive", "attachmentId"]
        for field in required_fields:
            if field not in thread_info:
                print(f"❌ FAILED: Missing '{field}' field in thread info")
                return False

        print(f"✅ PASSED: Thread status endpoint for '{thread_name}' working correctly")
        return True

    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    # Test mainloop thread
    success1 = test_thread_status("mainloop")
    print()

    # Test http_server thread
    success2 = test_thread_status("http_server")

    sys.exit(0 if (success1 and success2) else 1)