
#!/usr/bin/env python3
"""
HTTP test for threads list endpoint (GET /api/threads)
"""

import requests
import sys
import json

BASE_URL = "http://0.0.0.0:5000"

def test_threads_list_endpoint():
    """Test GET /api/threads - Get all threads"""
    print("Testing threads list endpoint (GET /api/threads)...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/threads", timeout=5)
        
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
        
        threads = data["threads"]
        if not isinstance(threads, dict):
            print(f"❌ FAILED: 'threads' should be a dictionary")
            return False
        
        # Check for expected threads
        if "mainloop" not in threads:
            print(f"❌ FAILED: 'mainloop' thread not found")
            return False
        
        if "http_server" not in threads:
            print(f"❌ FAILED: 'http_server' thread not found")
            return False
        
        # Verify thread info structure
        for thread_name, thread_info in threads.items():
            required_fields = ["threadId", "state", "isAlive", "attachmentId"]
            for field in required_fields:
                if field not in thread_info:
                    print(f"❌ FAILED: Thread '{thread_name}' missing '{field}' field")
                    return False
        
        print(f"✅ PASSED: Threads list endpoint working correctly ({len(threads)} threads found)")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success = test_threads_list_endpoint()
    sys.exit(0 if success else 1)
