
#!/usr/bin/env python3
"""
HTTP Server Test Suite for ur-mavrouter
Tests all RPC endpoints and HTTP server functionality
"""

import unittest
import requests
import json
import time
import subprocess
import signal
import os
from typing import Optional

class MavRouterHTTPTest(unittest.TestCase):
    """Test suite for MAVLink Router HTTP server"""
    
    BASE_URL = "http://0.0.0.0:5000"
    ROUTER_PROCESS: Optional[subprocess.Popen] = None
    
    @classmethod
    def setUpClass(cls):
        """Start the MAVLink router before tests"""
        print("Starting ur-mavrouter with HTTP server...")
        
        # Build the router first
        build_cmd = ["make", "-C", "../pkg_src"]
        subprocess.run(build_cmd, check=True)
        
        # Start the router with HTTP enabled
        router_cmd = [
            "../pkg_src/build/ur-mavrouter",
            "--json-conf-file", "../pkg_src/config/router-config.json",
            "--stats-conf-file", "../pkg_src/config/statistics-only-config.json",
            "--http-conf-file", "../pkg_src/config/http-server-config.json"
        ]
        
        cls.ROUTER_PROCESS = subprocess.Popen(
            router_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=os.path.dirname(os.path.abspath(__file__))
        )
        
        # Wait for server to start
        print("Waiting for HTTP server to start...")
        max_retries = 30
        for i in range(max_retries):
            try:
                response = requests.get(f"{cls.BASE_URL}/", timeout=1)
                if response.status_code == 200:
                    print(f"HTTP server started successfully on {cls.BASE_URL}")
                    break
            except requests.exceptions.RequestException:
                time.sleep(1)
        else:
            cls.tearDownClass()
            raise RuntimeError("Failed to start HTTP server")
    
    @classmethod
    def tearDownClass(cls):
        """Stop the MAVLink router after tests"""
        if cls.ROUTER_PROCESS:
            print("Stopping ur-mavrouter...")
            cls.ROUTER_PROCESS.send_signal(signal.SIGTERM)
            try:
                cls.ROUTER_PROCESS.wait(timeout=5)
            except subprocess.TimeoutExpired:
                cls.ROUTER_PROCESS.kill()
                cls.ROUTER_PROCESS.wait()
            print("ur-mavrouter stopped")
    
    def test_01_root_endpoint(self):
        """Test GET / - Root endpoint"""
        response = requests.get(f"{self.BASE_URL}/")
        
        self.assertEqual(response.status_code, 200)
        self.assertIn("text/html", response.headers.get("Content-Type", ""))
        self.assertIn("MAVLink Router HTTP Server", response.text)
        print("✓ Root endpoint test passed")
    
    def test_02_status_endpoint(self):
        """Test GET /status - Status endpoint"""
        response = requests.get(f"{self.BASE_URL}/status")
        
        self.assertEqual(response.status_code, 200)
        self.assertIn("application/json", response.headers.get("Content-Type", ""))
        
        data = response.json()
        self.assertIn("status", data)
        self.assertEqual(data["status"], "running")
        self.assertIn("service", data)
        self.assertEqual(data["service"], "mavlink-router")
        print("✓ Status endpoint test passed")
    
    def test_03_get_all_threads(self):
        """Test GET /api/threads - Get all thread status"""
        response = requests.get(f"{self.BASE_URL}/api/threads")
        
        self.assertEqual(response.status_code, 200)
        self.assertIn("application/json", response.headers.get("Content-Type", ""))
        
        data = response.json()
        self.assertIn("status", data)
        self.assertIn("message", data)
        self.assertIn("threads", data)
        
        # Verify threads structure
        threads = data["threads"]
        self.assertIsInstance(threads, dict)
        
        # Should have mainloop and http_server threads
        self.assertIn("mainloop", threads)
        self.assertIn("http_server", threads)
        
        # Verify thread info structure
        for thread_name, thread_info in threads.items():
            self.assertIn("threadId", thread_info)
            self.assertIn("state", thread_info)
            self.assertIn("isAlive", thread_info)
            self.assertIn("attachmentId", thread_info)
        
        print(f"✓ Get all threads test passed - Found {len(threads)} threads")
    
    def test_04_get_mainloop_thread(self):
        """Test GET /api/threads/mainloop - Get mainloop thread status"""
        response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        
        self.assertEqual(response.status_code, 200)
        data = response.json()
        
        self.assertIn("threads", data)
        self.assertIn("mainloop", data["threads"])
        
        mainloop_info = data["threads"]["mainloop"]
        self.assertIn("threadId", mainloop_info)
        self.assertIn("state", mainloop_info)
        self.assertIn("isAlive", mainloop_info)
        self.assertTrue(mainloop_info["isAlive"])
        
        print(f"✓ Get mainloop thread test passed - Thread ID: {mainloop_info['threadId']}")
    
    def test_05_get_http_server_thread(self):
        """Test GET /api/threads/http_server - Get HTTP server thread status"""
        response = requests.get(f"{self.BASE_URL}/api/threads/http_server")
        
        self.assertEqual(response.status_code, 200)
        data = response.json()
        
        self.assertIn("threads", data)
        self.assertIn("http_server", data["threads"])
        
        http_info = data["threads"]["http_server"]
        self.assertIn("threadId", http_info)
        self.assertIn("state", http_info)
        self.assertIn("isAlive", http_info)
        self.assertTrue(http_info["isAlive"])
        
        print(f"✓ Get HTTP server thread test passed - Thread ID: {http_info['threadId']}")
    
    def test_06_pause_mainloop_thread(self):
        """Test POST /api/threads/mainloop/pause - Pause mainloop thread"""
        response = requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        
        self.assertEqual(response.status_code, 200)
        data = response.json()
        
        self.assertIn("status", data)
        self.assertIn("message", data)
        
        # Verify thread is paused
        time.sleep(0.5)
        status_response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        status_data = status_response.json()
        mainloop_info = status_data["threads"]["mainloop"]
        
        # State 2 is Paused (from ThreadState enum)
        self.assertEqual(mainloop_info["state"], 2)
        
        print("✓ Pause mainloop thread test passed")
    
    def test_07_resume_mainloop_thread(self):
        """Test POST /api/threads/mainloop/resume - Resume mainloop thread"""
        response = requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        
        self.assertEqual(response.status_code, 200)
        data = response.json()
        
        self.assertIn("status", data)
        self.assertIn("message", data)
        
        # Verify thread is running
        time.sleep(0.5)
        status_response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        status_data = status_response.json()
        mainloop_info = status_data["threads"]["mainloop"]
        
        # State 1 is Running (from ThreadState enum)
        self.assertEqual(mainloop_info["state"], 1)
        
        print("✓ Resume mainloop thread test passed")
    
    def test_08_invalid_endpoint(self):
        """Test invalid endpoint returns 404"""
        response = requests.get(f"{self.BASE_URL}/api/invalid")
        
        self.assertEqual(response.status_code, 404)
        self.assertIn("Not Found", response.text)
        
        print("✓ Invalid endpoint test passed")
    
    def test_09_thread_state_transitions(self):
        """Test complete thread state transition cycle"""
        # Get initial state
        response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        initial_data = response.json()
        initial_state = initial_data["threads"]["mainloop"]["state"]
        
        # Pause
        requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        time.sleep(0.3)
        
        response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        paused_data = response.json()
        paused_state = paused_data["threads"]["mainloop"]["state"]
        self.assertEqual(paused_state, 2)  # Paused
        
        # Resume
        requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        time.sleep(0.3)
        
        response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        resumed_data = response.json()
        resumed_state = resumed_data["threads"]["mainloop"]["state"]
        self.assertEqual(resumed_state, 1)  # Running
        
        print("✓ Thread state transitions test passed")
    
    def test_10_concurrent_requests(self):
        """Test handling of concurrent requests"""
        import concurrent.futures
        
        def make_request():
            response = requests.get(f"{self.BASE_URL}/api/threads")
            return response.status_code == 200
        
        # Make 10 concurrent requests
        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
            futures = [executor.submit(make_request) for _ in range(10)]
            results = [f.result() for f in concurrent.futures.as_completed(futures)]
        
        # All requests should succeed
        self.assertEqual(sum(results), 10)
        print("✓ Concurrent requests test passed")
    
    def test_11_http_methods(self):
        """Test that endpoints respond correctly to different HTTP methods"""
        # GET should work on status endpoints
        get_response = requests.get(f"{self.BASE_URL}/api/threads")
        self.assertEqual(get_response.status_code, 200)
        
        # POST should work on control endpoints
        post_response = requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        self.assertEqual(post_response.status_code, 200)
        
        # Resume to restore state
        requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        
        print("✓ HTTP methods test passed")
    
    def test_12_json_response_format(self):
        """Test that all JSON responses are well-formed"""
        endpoints = [
            "/status",
            "/api/threads",
            "/api/threads/mainloop",
            "/api/threads/http_server"
        ]
        
        for endpoint in endpoints:
            response = requests.get(f"{self.BASE_URL}{endpoint}")
            self.assertEqual(response.status_code, 200)
            
            # Should be valid JSON
            try:
                data = response.json()
                self.assertIsInstance(data, dict)
            except json.JSONDecodeError:
                self.fail(f"Invalid JSON response from {endpoint}")
        
        print("✓ JSON response format test passed")
    
    def test_13_response_headers(self):
        """Test that responses have correct headers"""
        # JSON endpoints should have application/json content type
        response = requests.get(f"{self.BASE_URL}/api/threads")
        self.assertIn("application/json", response.headers.get("Content-Type", ""))
        
        # HTML endpoint should have text/html content type
        response = requests.get(f"{self.BASE_URL}/")
        self.assertIn("text/html", response.headers.get("Content-Type", ""))
        
        print("✓ Response headers test passed")


class HTTPServerPerformanceTest(unittest.TestCase):
    """Performance and stress tests for HTTP server"""
    
    BASE_URL = "http://0.0.0.0:5000"
    
    def test_response_time(self):
        """Test that responses are reasonably fast"""
        start_time = time.time()
        response = requests.get(f"{self.BASE_URL}/api/threads")
        elapsed = time.time() - start_time
        
        self.assertEqual(response.status_code, 200)
        self.assertLess(elapsed, 0.5)  # Should respond in less than 500ms
        
        print(f"✓ Response time test passed - {elapsed*1000:.2f}ms")
    
    def test_rapid_fire_requests(self):
        """Test server handles rapid sequential requests"""
        success_count = 0
        for _ in range(50):
            response = requests.get(f"{self.BASE_URL}/status")
            if response.status_code == 200:
                success_count += 1
        
        self.assertEqual(success_count, 50)
        print("✓ Rapid fire requests test passed - 50/50 successful")


if __name__ == "__main__":
    # Run tests with verbose output
    unittest.main(verbosity=2)
