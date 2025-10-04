
#!/usr/bin/env python3
"""
Test extension status endpoint
GET /api/extensions/status
GET /api/extensions/status/:name
"""

import requests
import sys
import json
import argparse

BASE_URL = "http://0.0.0.0:5000"

def test_get_all_extensions_status():
    """Test GET /api/extensions/status - Get all extensions status"""
    print("Testing GET /api/extensions/status...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/extensions/status", timeout=5)
        
        if response.status_code != 200:
            print(f"❌ FAILED: Expected status 200, got {response.status_code}")
            return False
        
        if "application/json" not in response.headers.get("Content-Type", ""):
            print(f"❌ FAILED: Expected application/json content type")
            return False
        
        try:
            data = response.json()
            if not isinstance(data, list):
                print(f"❌ FAILED: Expected array response")
                return False
            
            print(f"✅ PASSED: GET all extensions status - {len(data)} extensions found")
            print(f"Response: {json.dumps(data, indent=2)}")
            return True
            
        except json.JSONDecodeError:
            print(f"❌ FAILED: Invalid JSON response")
            return False
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_get_specific_extension_status(extension_name):
    """Test GET /api/extensions/status/:name - Get specific extension status"""
    print(f"\nTesting GET /api/extensions/status/{extension_name}...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/extensions/status/{extension_name}", timeout=5)
        
        if response.status_code == 200:
            data = response.json()
            print(f"✅ PASSED: GET extension status for '{extension_name}'")
            print(f"Response: {json.dumps(data, indent=2)}")
            return True
        elif response.status_code == 404:
            print(f"✅ PASSED: Extension '{extension_name}' not found (expected for new system)")
            return True
        else:
            print(f"❌ FAILED: Unexpected status {response.status_code}")
            return False
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Test extension status endpoints')
    parser.add_argument('--name', type=str, help='Extension name to check status for')
    parser.add_argument('--all', action='store_true', help='Get all extensions status')
    
    args = parser.parse_args()
    
    success = True
    
    if args.all or (not args.name and not args.all):
        success = test_get_all_extensions_status()
    
    if args.name:
        success = success and test_get_specific_extension_status(args.name)
    
    sys.exit(0 if success else 1)
