
#!/usr/bin/env python3
"""
MAVLink Endpoint Connectivity Monitor
Monitors real-time availability of MAVLink endpoints and validates MAVLink compatibility
"""

import socket
import json
import time
import struct
import sys
from datetime import datetime
from typing import Dict, List, Optional, Tuple
from enum import Enum

class EndpointType(Enum):
    UDP = "udp"
    TCP = "tcp"

class EndpointStatus(Enum):
    DISCONNECTED = "DISCONNECTED"
    CONNECTING = "CONNECTING"
    CONNECTED = "CONNECTED"
    MAVLINK_VERIFIED = "MAVLINK_VERIFIED"
    ERROR = "ERROR"

# MAVLink v2.0 magic byte
MAVLINK_V2_MAGIC = 0xFD
# MAVLink v1.0 magic byte
MAVLINK_V1_MAGIC = 0xFE

class MAVLinkEndpoint:
    def __init__(self, name: str, endpoint_type: str, address: str, port: int, enabled: bool = True):
        self.name = name
        self.type = EndpointType(endpoint_type)
        self.address = address
        self.port = port
        self.enabled = enabled
        self.status = EndpointStatus.DISCONNECTED
        self.socket: Optional[socket.socket] = None
        self.last_heartbeat = None
        self.error_message = ""
        self.mavlink_messages_received = 0
        self.last_reconnect_attempt = 0
        
    def connect(self) -> bool:
        """Attempt to connect to the endpoint"""
        if not self.enabled:
            return False
            
        try:
            self.status = EndpointStatus.CONNECTING
            
            if self.type == EndpointType.UDP:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.socket.settimeout(0.5)
                self.socket.setblocking(False)
                # For UDP, we consider it "connected" immediately
                self.status = EndpointStatus.CONNECTED
                return True
                
            elif self.type == EndpointType.TCP:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.settimeout(2.0)
                self.socket.connect((self.address, self.port))
                self.socket.setblocking(False)
                self.status = EndpointStatus.CONNECTED
                return True
                
        except Exception as e:
            self.error_message = str(e)
            self.status = EndpointStatus.ERROR
            self.close()
            return False
            
    def close(self):
        """Close the socket connection"""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            
    def receive_data(self) -> bytes:
        """Receive data from the endpoint"""
        if not self.socket:
            return b''
            
        try:
            if self.type == EndpointType.UDP:
                data, _ = self.socket.recvfrom(4096)
                return data
            else:  # TCP
                return self.socket.recv(4096)
        except socket.error:
            return b''
            
    def verify_mavlink(self, data: bytes) -> bool:
        """Verify if received data contains valid MAVLink messages"""
        if len(data) < 6:
            return False
            
        # Check for MAVLink v2.0 or v1.0 magic byte
        for i in range(len(data)):
            if data[i] == MAVLINK_V2_MAGIC or data[i] == MAVLINK_V1_MAGIC:
                self.mavlink_messages_received += 1
                self.last_heartbeat = time.time()
                self.status = EndpointStatus.MAVLINK_VERIFIED
                return True
        return False

class EndpointMonitor:
    def __init__(self, config_file: str):
        self.config_file = config_file
        self.endpoints: List[MAVLinkEndpoint] = []
        self.test_settings = {}
        self.running = True
        
    def load_config(self) -> bool:
        """Load endpoint configuration from JSON file"""
        try:
            with open(self.config_file, 'r') as f:
                config = json.load(f)
                
            self.test_settings = config.get('test_settings', {})
            
            for ep_config in config.get('endpoints', []):
                endpoint = MAVLinkEndpoint(
                    name=ep_config['name'],
                    endpoint_type=ep_config['type'],
                    address=ep_config['address'],
                    port=ep_config['port'],
                    enabled=ep_config.get('enabled', True)
                )
                self.endpoints.append(endpoint)
                
            return True
        except Exception as e:
            print(f"ERROR: Failed to load config: {e}")
            return False
            
    def display_status(self):
        """Display current status of all endpoints"""
        print("\n" + "="*80)
        print(f"MAVLink Endpoint Monitor - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("="*80)
        
        for ep in self.endpoints:
            status_color = {
                EndpointStatus.DISCONNECTED: "\033[90m",  # Gray
                EndpointStatus.CONNECTING: "\033[93m",    # Yellow
                EndpointStatus.CONNECTED: "\033[94m",     # Blue
                EndpointStatus.MAVLINK_VERIFIED: "\033[92m",  # Green
                EndpointStatus.ERROR: "\033[91m"          # Red
            }.get(ep.status, "")
            
            reset_color = "\033[0m"
            
            print(f"\n[{ep.name}] {ep.type.value.upper()} {ep.address}:{ep.port}")
            print(f"  Status: {status_color}{ep.status.value}{reset_color}")
            
            if ep.status == EndpointStatus.ERROR:
                print(f"  Error: {ep.error_message}")
            elif ep.status == EndpointStatus.MAVLINK_VERIFIED:
                print(f"  MAVLink Messages: {ep.mavlink_messages_received}")
                if ep.last_heartbeat:
                    age = time.time() - ep.last_heartbeat
                    print(f"  Last Message: {age:.1f}s ago")
                    
        print("\n" + "="*80)
        
    def monitor_endpoint(self, endpoint: MAVLinkEndpoint):
        """Monitor a single endpoint"""
        # Check if we need to connect/reconnect
        if endpoint.status in [EndpointStatus.DISCONNECTED, EndpointStatus.ERROR]:
            current_time = time.time()
            reconnect_delay = self.test_settings.get('reconnect_delay_seconds', 2)
            
            if current_time - endpoint.last_reconnect_attempt >= reconnect_delay:
                endpoint.last_reconnect_attempt = current_time
                endpoint.connect()
                
        # If connected, try to receive and verify MAVLink data
        if endpoint.status in [EndpointStatus.CONNECTED, EndpointStatus.MAVLINK_VERIFIED]:
            try:
                data = endpoint.receive_data()
                if data:
                    endpoint.verify_mavlink(data)
                    
                # Check for timeout
                if endpoint.last_heartbeat:
                    timeout = self.test_settings.get('heartbeat_timeout_seconds', 5)
                    if time.time() - endpoint.last_heartbeat > timeout:
                        endpoint.status = EndpointStatus.CONNECTED
                        
            except Exception as e:
                endpoint.error_message = str(e)
                endpoint.status = EndpointStatus.ERROR
                endpoint.close()
                
    def run(self):
        """Main monitoring loop"""
        if not self.load_config():
            return 1
            
        print("\nStarting MAVLink Endpoint Monitor...")
        print(f"Monitoring {len(self.endpoints)} endpoints")
        print("Press Ctrl+C to stop\n")
        
        status_interval = self.test_settings.get('status_interval_seconds', 1)
        last_status_time = 0
        
        try:
            while self.running:
                current_time = time.time()
                
                # Monitor all endpoints
                for endpoint in self.endpoints:
                    if endpoint.enabled:
                        self.monitor_endpoint(endpoint)
                        
                # Display status at regular intervals
                if current_time - last_status_time >= status_interval:
                    self.display_status()
                    last_status_time = current_time
                    
                time.sleep(0.1)  # Small sleep to prevent CPU spinning
                
        except KeyboardInterrupt:
            print("\n\nStopping monitor...")
            
        finally:
            # Cleanup
            for endpoint in self.endpoints:
                endpoint.close()
                
        return 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python endpoint_monitor.py <config_file.json>")
        print("\nExample:")
        print("  python endpoint_monitor.py config/endpoints.json")
        return 1
        
    monitor = EndpointMonitor(sys.argv[1])
    return monitor.run()

if __name__ == "__main__":
    sys.exit(main())
