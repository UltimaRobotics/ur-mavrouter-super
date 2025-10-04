#!/usr/bin/env python3
"""
HTTP test for mainloop thread start operation
Sends a start request to the mainloop thread
"""

import requests
import sys

BASE_URL = "http://0.0.0.0:5000"

def test_start_mainloop_thread():
    """Test POST /api/threads/mainloop/start - Start mainloop thread"""
    print("Sending mainloop thread start request...")

    try:
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/start", timeout=10)

        if response.status_code == 200:
            print(f"✅ Request sent successfully")
            print(f"   Status Code: {response.status_code}")
            print(f"   Response: {response.text}")
            return True
        else:
            print(f"❌ Request failed with status code: {response.status_code}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"❌ Request error - {e}")
        return False

if __name__ == "__main__":
    print("=" * 60)
    print("Mainloop Thread Start Test")
    print("=" * 60)

    success = test_start_mainloop_thread()

    print("\n" + "=" * 60)
    if success:
        print("✅ Test completed")
        print("=" * 60)
        sys.exit(0)
    else:
        print("❌ Test failed")
        print("=" * 60)
        sys.exit(1)
