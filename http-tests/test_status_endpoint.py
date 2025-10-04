
#!/usr/bin/env python3
"""
HTTP test for status endpoint (/status)
"""

import requests
import sys
import json

BASE_URL = "http://0.0.0.0:5000"

def test_status_endpoint():
    """Test GET /status - Status endpoint"""
    print("Testing status endpoint (/status)...")
    
    try:
        response = requests.get(f"{BASE_URL}/status", timeout=5)
        
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
        
        # Check required fields
        if "status" not in data or data["status"] != "running":
            print(f"❌ FAILED: Expected status field with value 'running'")
            return False
        
        if "service" not in data or data["service"] != "mavlink-router":
            print(f"❌ FAILED: Expected service field with value 'mavlink-router'")
            return False
        
        print("✅ PASSED: Status endpoint working correctly")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

if __name__ == "__main__":
    success = test_status_endpoint()
    sys.exit(0 if success else 1)
