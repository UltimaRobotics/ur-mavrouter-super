
#!/usr/bin/env python3
"""
HTTP test for root endpoint (/)
"""

import requests
import sys

BASE_URL = "http://0.0.0.0:5000"

def test_root_endpoint():
    """Test GET / - Root endpoint"""
    print("Testing root endpoint (/)...")
    
    try:
        response = requests.get(f"{BASE_URL}/", timeout=5)
        
        # Check status code
        if response.status_code != 200:
            print(f"❌ FAILED: Expected status 200, got {response.status_code}")
            return False
        
        # Check content type
        if "text/html" not in response.headers.get("Content-Type", ""):
            print(f"❌ FAILED: Expected text/html content type")
            return False
        
        # Check content
        if "MAVLink Router HTTP Server" not in response.text:
            print(f"❌ FAILED: Expected 'MAVLink Router HTTP Server' in response")
            return False
        
        print("✅ PASSED: Root endpoint working correctly")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success = test_root_endpoint()
    sys.exit(0 if success else 1)
