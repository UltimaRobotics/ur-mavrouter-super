replit_final_file>
# HTTP Endpoint Tests for ur-mavrouter

Simple HTTP client tests for the ur-mavrouter HTTP server. Each endpoint has its own dedicated test script.

## Test Files

- **test_root_endpoint.py** - Tests root endpoint (`/`)
- **test_status_endpoint.py** - Tests status endpoint (`/status`)
- **test_threads_list_endpoint.py** - Tests listing all threads (`GET /api/threads`)
- **test_thread_status_endpoint.py** - Tests individual thread status (`GET /api/threads/{name}`)
- **test_thread_pause_endpoint.py** - Tests pausing threads (`POST /api/threads/{name}/pause`)
- **test_thread_resume_endpoint.py** - Tests resuming threads (`POST /api/threads/{name}/resume`)
- **test_thread_stop_endpoint.py** - Tests stopping threads (`POST /api/threads/{name}/stop`)
- **test_error_handling.py** - Tests error handling (404s, invalid methods)
- **test_performance.py** - Performance and stress tests

## Prerequisites

1. Install Python requests library:
```bash
pip install requests
```

2. Start ur-mavrouter with HTTP server enabled:
```bash
cd ../pkg_src
make
./build/ur-mavrouter --json-conf-file config/router-config.json \
                      --stats-conf-file config/statistics-only-config.json \
                      --http-conf-file config/http-server-config.json
```

## Running Tests

### Run All Tests
```bash
chmod +x run_all_tests.sh
./run_all_tests.sh
```

### Run Individual Test
```bash
python3 test_root_endpoint.py
python3 test_status_endpoint.py
python3 test_threads_list_endpoint.py
# ... etc
```

## Test Output

Each test outputs:
- ✅ PASSED: Test succeeded
- ❌ FAILED: Test failed with reason

Exit codes:
- 0: Success
- 1: Failure

</replit_final_file>