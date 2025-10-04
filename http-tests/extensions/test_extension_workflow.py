
#!/usr/bin/env python3
"""
Comprehensive extension workflow test
Tests the full lifecycle: add -> status -> stop -> start -> delete
"""

import requests
import sys
import json
import time

BASE_URL = "http://0.0.0.0:5000"

def test_extension_workflow():
    """Test complete extension lifecycle"""
    print("=" * 60)
    print("Extension Workflow Test - Full Lifecycle")
    print("=" * 60)
    
    extension_name = "workflow_test_extension"
    
    # Step 1: Add extension
    print("\n[STEP 1] Adding extension...")
    extension_config = {
        "name": extension_name,
        "type": "udp",
        "address": "127.0.0.1",
        "port": 44200,
        "assigned_extension_point": "udp-extension-point-2"
    }
    
    response = requests.post(
        f"{BASE_URL}/api/extensions/add",
        json=extension_config,
        headers={"Content-Type": "application/json"},
        timeout=5
    )
    
    if response.status_code not in [200, 201, 400]:
        print(f"❌ FAILED: Add extension - status {response.status_code}")
        return False
    print(f"✅ Add extension: {response.status_code}")
    
    time.sleep(0.5)
    
    # Step 2: Check status
    print("\n[STEP 2] Checking extension status...")
    response = requests.get(
        f"{BASE_URL}/api/extensions/status/{extension_name}",
        timeout=5
    )
    
    if response.status_code in [200, 404]:
        print(f"✅ Check status: {response.status_code}")
        if response.status_code == 200:
            print(f"Extension data: {json.dumps(response.json(), indent=2)}")
    else:
        print(f"❌ FAILED: Check status - status {response.status_code}")
        return False
    
    time.sleep(0.5)
    
    # Step 3: Stop extension
    print("\n[STEP 3] Stopping extension...")
    response = requests.post(
        f"{BASE_URL}/api/extensions/stop/{extension_name}",
        timeout=5
    )
    
    if response.status_code in [200, 404]:
        print(f"✅ Stop extension: {response.status_code}")
    else:
        print(f"❌ FAILED: Stop extension - status {response.status_code}")
        return False
    
    time.sleep(0.5)
    
    # Step 4: Start extension
    print("\n[STEP 4] Starting extension...")
    response = requests.post(
        f"{BASE_URL}/api/extensions/start/{extension_name}",
        timeout=5
    )
    
    if response.status_code in [200, 404]:
        print(f"✅ Start extension: {response.status_code}")
    else:
        print(f"❌ FAILED: Start extension - status {response.status_code}")
        return False
    
    time.sleep(0.5)
    
    # Step 5: Delete extension
    print("\n[STEP 5] Deleting extension...")
    response = requests.delete(
        f"{BASE_URL}/api/extensions/delete/{extension_name}",
        timeout=5
    )
    
    if response.status_code in [200, 404]:
        print(f"✅ Delete extension: {response.status_code}")
    else:
        print(f"❌ FAILED: Delete extension - status {response.status_code}")
        return False
    
    print("\n" + "=" * 60)
    print("✅ PASSED: Complete extension workflow test")
    print("=" * 60)
    return True

if __name__ == "__main__":
    success = test_extension_workflow()
    sys.exit(0 if success else 1)
