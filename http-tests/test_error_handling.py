
#!/usr/bin/env python3
"""
HTTP test for error handling (404s, invalid methods)
"""

import requests
import sys

BASE_URL = "http://0.0.0.0:5000"

def test_invalid_endpoint():
    """Test that invalid endpoint returns 404"""
    print("Testing invalid endpoint error handling...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/invalid", timeout=5)
        
        if response.status_code != 404:
            print(f"❌ FAILED: Expected status 404, got {response.status_code}")
            return False
        
        if "Not Found" not in response.text:
            print(f"❌ FAILED: Expected 'Not Found' in response")
            return False
        
        print("✅ PASSED: Invalid endpoint returns 404")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_invalid_path():
    """Test that invalid API path returns 404"""
    print("Testing invalid API path error handling...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/nonexistent/path", timeout=5)
        
        if response.status_code != 404:
            print(f"❌ FAILED: Expected status 404, got {response.status_code}")
            return False
        
        print("✅ PASSED: Invalid API path returns 404")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_wrong_http_method():
    """Test using wrong HTTP method"""
    print("Testing wrong HTTP method error handling...")
    
    try:
        # Try POST on a GET-only endpoint
        response = requests.post(f"{BASE_URL}/api/threads", timeout=5)
        
        if response.status_code != 404:
            print(f"❌ FAILED: Expected status 404, got {response.status_code}")
            return False
        
        print("✅ PASSED: Wrong HTTP method returns 404")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success1 = test_invalid_endpoint()
    print()
    success2 = test_invalid_path()
    print()
    success3 = test_wrong_http_method()
    
    sys.exit(0 if (success1 and success2 and success3) else 1)
