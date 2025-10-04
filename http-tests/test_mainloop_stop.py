#!/usr/bin/env python3
"""
HTTP test for mainloop thread stop operation
Sends a stop request to the mainloop thread
"""

import requests
import sys

BASE_URL = "http://0.0.0.0:5000"

def test_stop_mainloop_thread():
    """Test POST /api/threads/mainloop/stop - Stop only mainloop thread"""
    print("Sending mainloop thread stop request...")

    try:
        response = requests.post(f"{BASE_URL}/api/threads/mainloop/stop", timeout=10)

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
    print("Mainloop Thread Stop Test")
    print("=" * 60)

    success = test_stop_mainloop_thread()

    print("\n" + "=" * 60)
    if success:
        print("✅ Test completed")
        print("=" * 60)
        sys.exit(0)
    else:
        print("❌ Test failed")
        print("=" * 60)
        sys.exit(1)
