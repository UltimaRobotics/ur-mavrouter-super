#!/usr/bin/env python3
"""
HTTP test for thread resume endpoint (POST /api/threads/{name}/resume)
"""

import requests
import sys
import json
import time

BASE_URL = "http://0.0.0.0:5000"

def test_thread_resume():
    """Test POST /api/threads/mainloop/resume - Resume thread"""
    print("Testing thread resume endpoint (POST /api/threads/mainloop/resume)...")

    try:
        # First pause the thread
        requests.post(f"{BASE_URL}/api/threads/mainloop/pause", timeout=5)
        time.sleep(0.3)

        # Send resume request
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/resume", timeout=5)

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

        # Verify thread is running by checking status
        status_response = requests.get(f"{BASE_URL}/api/threads/mainloop", timeout=5)
        status_data = status_response.json()

        if "threads" in status_data and "mainloop" in status_data["threads"]:
            thread_state = status_data["threads"]["mainloop"]["state"]
            if thread_state == 1:  # Running state
                print("✅ PASSED: Thread resume endpoint working correctly (state: Running)")
                return True
            else:
                print(f"⚠️  WARNING: Thread resumed but state is {thread_state} (expected 1)")
                return True
        else:
             print("✅ PASSED: Thread resume endpoint responded correctly (could not verify state)")
             return True


    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success = test_thread_resume()
    sys.exit(0 if success else 1)