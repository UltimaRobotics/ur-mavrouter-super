
#!/usr/bin/env python3
"""
HTTP test for thread stop endpoint (POST /api/threads/{name}/stop)
"""

import requests
import sys
import json

BASE_URL = "http://0.0.0.0:5000"

def test_thread_stop():
    """Test POST /api/threads/mainloop/stop - Stop thread"""
    print("Testing thread stop endpoint (POST /api/threads/mainloop/stop)...")
    
    try:
        # Send stop request
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/stop", timeout=5)
        
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
        if "status" not in data or "message" not in data:
            print(f"❌ FAILED: Missing 'status' or 'message' field in response")
            return False
        
        print("✅ PASSED: Thread stop endpoint responded correctly")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_stop_all_threads():
    """Test POST /api/threads/all/stop - Stop all threads"""
    print("Testing stop all threads endpoint (POST /api/threads/all/stop)...")
    
    try:
        # Send stop all request
        response = requests.post(f"{BASE_URL}/api/threads/all/stop", timeout=5)
        
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
        if "status" not in data or "message" not in data:
            print(f"❌ FAILED: Missing 'status' or 'message' field in response")
            return False
        
        print("✅ PASSED: Stop all threads endpoint responded correctly")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success1 = test_thread_stop()
    print()
    success2 = test_stop_all_threads()
    
    sys.exit(0 if (success1 and success2) else 1)
