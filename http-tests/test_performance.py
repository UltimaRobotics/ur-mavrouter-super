
#!/usr/bin/env python3
"""
HTTP performance tests
"""

import requests
import sys
import time
import concurrent.futures

BASE_URL = "http://0.0.0.0:5000"

def test_response_time():
    """Test that responses are reasonably fast"""
    print("Testing response time...")
    
    try:
        start_time = time.time()
        response = requests.get(f"{BASE_URL}/api/threads", timeout=5)
        elapsed = time.time() - start_time
        
        if response.status_code != 200:
            print(f"❌ FAILED: Expected status 200, got {response.status_code}")
            return False
        
        if elapsed > 0.5:
            print(f"❌ FAILED: Response too slow ({elapsed*1000:.2f}ms > 500ms)")
            return False
        
        print(f"✅ PASSED: Response time OK ({elapsed*1000:.2f}ms)")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_sequential_requests():
    """Test server handles rapid sequential requests"""
    print("Testing rapid sequential requests (50 requests)...")
    
    try:
        success_count = 0
        for i in range(50):
            response = requests.get(f"{BASE_URL}/status", timeout=5)
            if response.status_code == 200:
                success_count += 1
        
        if success_count != 50:
            print(f"❌ FAILED: Only {success_count}/50 requests succeeded")
            return False
        
        print(f"✅ PASSED: All 50 sequential requests succeeded")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ FAILED: Request error - {e}")
        return False

def test_concurrent_requests():
    """Test handling of concurrent requests"""
    print("Testing concurrent requests (10 parallel)...")
    
    def make_request():
        response = requests.get(f"{BASE_URL}/api/threads", timeout=5)
        return response.status_code == 200
    
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
            futures = [executor.submit(make_request) for _ in range(10)]
            results = [f.result() for f in concurrent.futures.as_completed(futures)]
        
        success_count = sum(results)
        
        if success_count != 10:
            print(f"❌ FAILED: Only {success_count}/10 concurrent requests succeeded")
            return False
        
        print(f"✅ PASSED: All 10 concurrent requests succeeded")
        return True
        
    except Exception as e:
        print(f"❌ FAILED: Error - {e}")
        return False

if __name__ == "__main__":
    success1 = test_response_time()
    print()
    success2 = test_sequential_requests()
    print()
    success3 = test_concurrent_requests()
    
    sys.exit(0 if (success1 and success2 and success3) else 1)
