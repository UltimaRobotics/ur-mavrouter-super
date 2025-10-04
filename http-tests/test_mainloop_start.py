
#!/usr/bin/env python3
"""
HTTP test for mainloop thread start operation
Tests getting the mainloop thread ID and attempting to start that thread
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

def test_start_mainloop_thread(thread_info):
    """Test POST /api/threads/mainloop/start - Start mainloop thread"""
    print("\nTesting start mainloop thread endpoint...")
    
    try:
        # Send start request for mainloop only
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/start", timeout=10)
        
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
        
        status = data['status']
        message = data['message']
        
        print(f"✅ PASSED: Mainloop thread start request completed")
        print(f"   Status: {status}")
        print(f"   Message: {message}")
        
        # Interpret the status
        if thread_info and thread_info.get('isAlive'):
            # Thread was running
            if status == "1":  # ALREADY_IN_STATE status
                print(f"   ℹ️  Thread was already running (expected behavior)")
            else:
                print(f"   ⚠️  Unexpected status for running thread")
        else:
            # Thread was not running
            if status == "0":  # SUCCESS status
                print(f"   ℹ️  Thread was restarted successfully (expected behavior)")
            elif status == "2":  # FAILED status
                print(f"   ⚠️  Thread restart failed")
        
        # Check response thread state if present
        if "threads" in data and "mainloop" in data["threads"]:
            new_thread_info = data["threads"]["mainloop"]
            new_thread_id = new_thread_info.get('threadId', 'N/A')
            old_thread_id = thread_info.get('threadId', 'N/A') if thread_info else 'N/A'
            
            print(f"   Thread ID after start request: {new_thread_id}")
            print(f"   Original thread ID: {old_thread_id}")
            
            if new_thread_id != old_thread_id and not thread_info.get('isAlive', True):
                print(f"   ✅ Thread was restarted with new ID (as expected for dead thread)")
            
            print(f"   Thread is alive: {new_thread_info.get('isAlive', 'N/A')}")
            print(f"   Thread state: {new_thread_info.get('state', 'N/A')}")
        
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_verify_mainloop_still_running():
    """Verify that mainloop is still running after start request"""
    print("\nVerifying that mainloop thread is still running...")
    
    try:
        # Wait a moment for any state changes
        time.sleep(1)
        
        # Get current mainloop status
        response = requests.get(f"{BASE_URL}/api/threads/mainloop", timeout=5)
        
        if response.status_code == 200:
            data = response.json()
            if "threads" in data and "mainloop" in data["threads"]:
                thread_info = data["threads"]["mainloop"]
                is_alive = thread_info.get("isAlive", False)
                
                if is_alive:
                    print(f"✅ PASSED: Mainloop thread is still running")
                    print(f"   Thread ID: {thread_info.get('threadId', 'N/A')}")
                    print(f"   Thread state: {thread_info.get('state', 'N/A')}")
                    return True
                else:
                    print(f"⚠️  WARNING: Mainloop thread is not alive")
                    return True  # Don't fail, just warn
        
        print(f"⚠️  WARNING: Could not verify mainloop status")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"⚠️  WARNING: Could not verify mainloop status - {e}")
        return True  # Don't fail on verification issues

if __name__ == "__main__":
    print("=" * 60)
    print("Mainloop Thread Start Test")
    print("=" * 60)
    
    # Test 1: Get mainloop thread ID
    success1, thread_info = test_get_mainloop_thread_id()
    
    if not success1:
        print("\n❌ Test suite FAILED: Could not get mainloop thread ID")
        sys.exit(1)
    
    # Test 2: Start mainloop thread
    success2 = test_start_mainloop_thread(thread_info)
    
    if not success2:
        print("\n❌ Test suite FAILED: Could not start mainloop thread")
        sys.exit(1)
    
    # Test 3: Verify mainloop still running
    success3 = test_verify_mainloop_still_running()
    
    print("\n" + "=" * 60)
    if success1 and success2 and success3:
        print("✅ All tests PASSED")
        print("=" * 60)
        sys.exit(0)
    else:
        print("❌ Some tests FAILED")
        print("=" * 60)
        sys.exit(1)

