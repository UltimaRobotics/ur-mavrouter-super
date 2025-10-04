
#!/usr/bin/env python3
"""
Test extension add endpoint
POST /api/extensions/add
"""

import requests
import sys
import json
import argparse

BASE_URL = "http://0.0.0.0:5000"

def test_add_extension(name, ext_type, address, port, extension_point):
    """Test POST /api/extensions/add - Add new extension"""
    print(f"Testing POST /api/extensions/add with name={name}...")
    
    # Extension configuration
    extension_config = {
        "name": name,
        "type": ext_type,
        "address": address,
        "port": port,
        "assigned_extension_point": extension_point
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/extensions/add",
            json=extension_config,
            headers={"Content-Type": "application/json"},
            timeout=5
        )
        
        if response.status_code in [200, 201]:
            print(f"✅ PASSED: Extension added successfully")
            try:
                data = response.json()
                print(f"Response: {json.dumps(data, indent=2)}")
            except:
                print(f"Response: {response.text}")
            return True
        elif response.status_code == 400:
            print(f"⚠️  WARNING: Extension may already exist or config invalid")
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
    parser = argparse.ArgumentParser(description='Test extension add endpoint')
    parser.add_argument('--name', type=str, default='test_extension_1', help='Extension name')
    parser.add_argument('--type', type=str, default='udp', help='Extension type (udp/tcp)')
    parser.add_argument('--address', type=str, default='127.0.0.1', help='Extension address')
    parser.add_argument('--port', type=int, default=44100, help='Extension port')
    parser.add_argument('--extension-point', type=str, default='udp-extension-point-1', 
                        help='Assigned extension point')
    
    args = parser.parse_args()
    
    success = test_add_extension(args.name, args.type, args.address, args.port, args.extension_point)
    
    sys.exit(0 if success else 1)
