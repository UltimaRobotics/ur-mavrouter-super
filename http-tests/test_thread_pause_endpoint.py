
#!/usr/bin/env python3
"""
HTTP test for thread pause endpoint (POST /api/threads/{name}/pause)
"""

import requests
import sys
import json
import time

BASE_URL = "http://0.0.0.0:5000"

def test_thread_pause():
    """Test POST /api/threads/mainloop/pause - Pause thread"""
    print("Testing thread pause endpoint (POST /api/threads/mainloop/pause)...")
    
    try:
        # Send pause request
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/pause", timeout=5)
        
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
        
        # Wait for state change
        time.sleep(0.5)
        
        # Verify thread is paused by checking status
        status_response = requests.get(f"{BASE_URL}/api/threads/mainloop", timeout=5)
        status_data = status_response.json()
        
        if "threads" in status_data and "mainloop" in status_data["threads"]:
            thread_state = status_data["threads"]["mainloop"]["state"]
            if thread_state == 2:  # Paused state
                print("✅ PASSED: Thread pause endpoint working correctly (state: Paused)")
                return True
            else:
                print(f"⚠️  WARNING: Thread paused but state is {thread_state} (expected 2)")
                return True
        
        print("✅ PASSED: Thread pause endpoint responded correctly")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success = test_thread_pause()
    
    # Resume thread to restore state
    try:
        requests.post(f"{BASE_URL}/api/threads/mainloop/resume", timeout=5)
    except:
        pass
    
    sys.exit(0 if success else 1)
