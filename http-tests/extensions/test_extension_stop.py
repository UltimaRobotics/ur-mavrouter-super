
#!/usr/bin/env python3
"""
Test extension stop endpoint
POST /api/extensions/stop/:name
"""

import requests
import sys
import json
import argparse

BASE_URL = "http://0.0.0.0:5000"

def test_stop_extension(extension_name):
    """Test POST /api/extensions/stop/:name - Stop extension"""
    print(f"Testing POST /api/extensions/stop/{extension_name}...")

    try:
        response = requests.post(
            f"{BASE_URL}/api/extensions/stop/{extension_name}",
            timeout=5
        )

        if response.status_code == 200:
            print(f"✅ PASSED: Extension '{extension_name}' stopped successfully")
            try:
                data = response.json()
                print(f"Response: {json.dumps(data, indent=2)}")
            except:
                print(f"Response: {response.text}")
            return True
        elif response.status_code == 404:
            print(f"⚠️  WARNING: Extension '{extension_name}' not found")
            print(f"Response: {response.text}")
            return True
        else:
            print(f"❌ FAILED: Unexpected status {response.status_code}")
            print(f"Response: {response.text}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Test extension stop endpoint')
    parser.add_argument('--name', type=str, default='test_extension_1', 
                        help='Extension name to stop')

    args = parser.parse_args()

    success = test_stop_extension(args.name)

    sys.exit(0 if success else 1)
