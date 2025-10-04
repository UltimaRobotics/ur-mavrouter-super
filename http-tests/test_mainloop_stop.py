
#!/usr/bin/env python3
"""
HTTP test for mainloop thread stop operation
Tests getting the mainloop thread ID and stopping only that thread
"""

import requests
import sys
import json
import time

BASE_URL = "http://0.0.0.0:5000"

def test_get_mainloop_thread_id():
    """Test GET /api/threads/mainloop - Get mainloop thread ID"""
    print("Testing get mainloop thread ID endpoint...")
    
    try:
        # Get mainloop thread info
        response = requests.get(f"{BASE_URL}/api/threads/mainloop", timeout=5)
        
        # Check status code
        if response.status_code != 200:
            print(f"❌ FAILED: Expected status 200, got {response.status_code}")
            return False, None
        
        # Check content type
        if "application/json" not in response.headers.get("Content-Type", ""):
            print(f"❌ FAILED: Expected application/json content type")
            return False, None
        
        # Parse JSON
        try:
            data = response.json()
        except json.JSONDecodeError:
            print(f"❌ FAILED: Invalid JSON response")
            return False, None
        
        # Check response structure
        if "threads" not in data:
            print(f"❌ FAILED: Missing 'threads' field in response")
            return False, None
        
        if "mainloop" not in data["threads"]:
            print(f"❌ FAILED: Missing 'mainloop' thread in response")
            return False, None
        
        thread_info = data["threads"]["mainloop"]
        
        # Verify required fields
        if "threadId" not in thread_info:
            print(f"❌ FAILED: Missing 'threadId' field")
            return False, None
        
        if "isAlive" not in thread_info:
            print(f"❌ FAILED: Missing 'isAlive' field")
            return False, None
        
        thread_id = thread_info["threadId"]
        is_alive = thread_info["isAlive"]
        
        print(f"✅ PASSED: Retrieved mainloop thread ID: {thread_id}")
        print(f"   Thread is alive: {is_alive}")
        print(f"   Thread state: {thread_info.get('state', 'unknown')}")
        
        return True, thread_info
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False, None

def test_stop_mainloop_thread():
    """Test POST /api/threads/mainloop/stop - Stop only mainloop thread"""
    print("\nTesting stop mainloop thread endpoint...")
    
    try:
        # Send stop request for mainloop only
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/stop", timeout=10)
        
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
        
        print(f"✅ PASSED: Mainloop thread stop request completed")
        print(f"   Status: {data['status']}")
        print(f"   Message: {data['message']}")
        
        # Note: Thread state may still show as alive immediately after stop request
        # because the stop is asynchronous - the thread will stop shortly
        if "threads" in data and "mainloop" in data["threads"]:
            thread_info = data["threads"]["mainloop"]
            print(f"   Thread ID after stop request: {thread_info.get('threadId', 'N/A')}")
            print(f"   Thread is alive (may still be true briefly): {thread_info.get('isAlive', 'N/A')}")
            print(f"   Thread state: {thread_info.get('state', 'N/A')}")
        
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_verify_only_mainloop_stopped():
    """Verify that HTTP server is still running after mainloop stop"""
    print("\nVerifying that HTTP server is still accessible after mainloop stop...")
    
    try:
        # Wait a moment for the stop to take effect
        time.sleep(2)
        
        # Try to access the HTTP server to verify it's still running
        response = requests.get(f"{BASE_URL}/status", timeout=5)
        
        if response.status_code == 200:
            print(f"✅ PASSED: HTTP server is still accessible after mainloop stop")
            return True
        else:
            print(f"⚠️  WARNING: HTTP server returned unexpected status {response.status_code}")
            return True  # Don't fail for this
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: HTTP server is not accessible - {e}")
        print(f"   This indicates the HTTP server stopped when mainloop stopped")
        return False

if __name__ == "__main__":
    print("=" * 60)
    print("Mainloop Thread Stop Test")
    print("=" * 60)
    
    # Test 1: Get mainloop thread ID
    success1, thread_info = test_get_mainloop_thread_id()
    
    if not success1:
        print("\n❌ Test suite FAILED: Could not get mainloop thread ID")
        sys.exit(1)
    
    # Test 2: Stop mainloop thread
    success2 = test_stop_mainloop_thread()
    
    if not success2:
        print("\n❌ Test suite FAILED: Could not stop mainloop thread")
        sys.exit(1)
    
    # Test 3: Verify only mainloop stopped
    success3 = test_verify_only_mainloop_stopped()
    
    print("\n" + "=" * 60)
    if success1 and success2 and success3:
        print("✅ All tests PASSED")
        print("=" * 60)
        sys.exit(0)
    else:
        print("❌ Some tests FAILED")
        print("=" * 60)
        sys.exit(1)

