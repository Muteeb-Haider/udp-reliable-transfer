# Robot Framework Tests for UDP Reliable Transfer

This directory contains Robot Framework tests for the UDP reliable transfer protocol implementation.

## Prerequisites

- Python 3.7 or higher
- Robot Framework
- Built UDP client and server binaries

## Installation

Install the required dependencies:

```bash
pip install -r requirements.txt
```

## Running Tests

### Run all tests:
```bash
python run_tests.py --install-requirements
```

### Run specific test categories:
```bash
# Basic functionality tests
python run_tests.py --tags basic

# Transfer tests
python run_tests.py --tags transfer

# Error handling tests
python run_tests.py --tags error
```

### Run with custom output directory:
```bash
python run_tests.py --output-dir ./my_test_results
```

### Run specific test file:
```bash
python run_tests.py --test-file test_udp_transfer.robot
```

## Test Categories

### Basic Tests (`basic`)
- Basic file transfer functionality
- Simple client-server communication

### Transfer Tests (`transfer`)
- File transfer with custom chunk sizes
- File transfer with custom window sizes
- File transfer with custom timeouts
- Multiple sequential file transfers

### Error Tests (`error`)
- Server error handling
- Invalid file requests
- Network error scenarios

## Test Structure

- `test_udp_transfer.robot`: Main test suite
- `libraries/UdpTransferLibrary.py`: Python library for test operations
- `run_tests.py`: Test runner script
- `requirements.txt`: Python dependencies

## Test Output

Test results are generated in the specified output directory (default: `./test_results`) and include:
- HTML test report
- XML test report
- Log file with detailed execution information

## Integration with Makefile

The tests are integrated with the project Makefile:

```bash
# Run all tests
make test

# Run specific test categories
make test-basic
make test-transfer
make test-error

# Clean test results
make clean
```

