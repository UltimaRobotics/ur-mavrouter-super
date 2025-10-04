
#!/usr/bin/env python3
"""
RPC Controller Test Suite
Tests the RPC mechanism for thread control
"""

import unittest
import requests
import json
import time

class RPCControllerTest(unittest.TestCase):
    """Test suite for RPC controller functionality"""
    
    BASE_URL = "http://0.0.0.0:5000"
    
    def setUp(self):
        """Ensure server is accessible before each test"""
        try:
            response = requests.get(f"{self.BASE_URL}/status", timeout=2)
            self.assertEqual(response.status_code, 200)
        except requests.exceptions.RequestException as e:
            self.fail(f"Server not accessible: {e}")
    
    def test_thread_registry(self):
        """Test that threads are properly registered"""
        response = requests.get(f"{self.BASE_URL}/api/threads")
        self.assertEqual(response.status_code, 200)
        
        data = response.json()
        threads = data.get("threads", {})
        
        # Should have registered threads
        self.assertGreater(len(threads), 0)
        
        # Each thread should have required fields
        for thread_name, thread_info in threads.items():
            self.assertIsInstance(thread_name, str)
            self.assertIn("threadId", thread_info)
            self.assertIn("state", thread_info)
            self.assertIn("isAlive", thread_info)
            self.assertIn("attachmentId", thread_info)
            
            # Thread ID should be positive
            self.assertGreater(thread_info["threadId"], 0)
            
            # Attachment ID should match thread name
            self.assertIn(thread_name, thread_info["attachmentId"])
        
        print(f"✓ Thread registry test passed - {len(threads)} threads registered")
    
    def test_thread_state_enum(self):
        """Test that thread states follow expected enum values"""
        response = requests.get(f"{self.BASE_URL}/api/threads/mainloop")
        data = response.json()
        
        mainloop_info = data["threads"]["mainloop"]
        state = mainloop_info["state"]
        
        # State should be one of: Created(0), Running(1), Paused(2), 
        # Stopped(3), Completed(4), Error(5)
        self.assertIn(state, [0, 1, 2, 3, 4, 5])
        
        print(f"✓ Thread state enum test passed - State: {state}")
    
    def test_pause_resume_idempotency(self):
        """Test that pause/resume operations are idempotent"""
        # Pause twice
        response1 = requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        self.assertEqual(response1.status_code, 200)
        
        time.sleep(0.3)
        
        response2 = requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        self.assertEqual(response2.status_code, 200)
        
        # Resume twice
        response3 = requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        self.assertEqual(response3.status_code, 200)
        
        time.sleep(0.3)
        
        response4 = requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        self.assertEqual(response4.status_code, 200)
        
        print("✓ Pause/resume idempotency test passed")
    
    def test_thread_alive_status(self):
        """Test that thread alive status is accurate"""
        response = requests.get(f"{self.BASE_URL}/api/threads")
        data = response.json()
        
        for thread_name, thread_info in data["threads"].items():
            # All registered threads should be alive
            self.assertTrue(
                thread_info["isAlive"],
                f"Thread {thread_name} is not alive but is registered"
            )
        
        print("✓ Thread alive status test passed")
    
    def test_rpc_response_structure(self):
        """Test that RPC responses have consistent structure"""
        # Test pause operation response
        response = requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        data = response.json()
        
        # Should have status and message
        self.assertIn("status", data)
        self.assertIn("message", data)
        
        # Status should be a number (OperationStatus enum)
        self.assertIsInstance(int(data["status"]), int)
        
        # Message should be a string
        self.assertIsInstance(data["message"], str)
        
        # Resume to restore state
        requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        
        print("✓ RPC response structure test passed")
    
    def test_operation_status_codes(self):
        """Test that operation status codes are meaningful"""
        # Successful operation
        response = requests.post(f"{self.BASE_URL}/api/threads/mainloop/pause")
        data = response.json()
        
        # Status 0 = SUCCESS
        self.assertEqual(int(data["status"]), 0)
        
        # Resume
        requests.post(f"{self.BASE_URL}/api/threads/mainloop/resume")
        
        print("✓ Operation status codes test passed")


if __name__ == "__main__":
    unittest.main(verbosity=2)
