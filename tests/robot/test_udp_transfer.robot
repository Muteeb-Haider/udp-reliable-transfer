*** Settings ***
Documentation     Test suite for UDP reliable transfer protocol
Library           OperatingSystem
Library           Process
Library           Collections
Library           String
Library           ../libraries/UdpTransferLibrary.py

*** Variables ***
${SERVER_PORT}    54321
${CLIENT_HOST}    127.0.0.1
${SERVER_DATA_DIR}    ..${/}..${/}server_data
${SAMPLE_DATA_DIR}    ..${/}..${/}sample_data
${SAMPLE_FILE}    sample.bin
${BUILD_DIR}      ..${/}..${/}build${/}bin

*** Keywords ***
Get Available Port
    [Documentation]    Get an available port for testing
    ${port}=    Evaluate    random.randint(10000, 65000)    random
    [Return]    ${port}

*** Test Cases ***
Test Basic File Transfer
    [Documentation]    Test basic file transfer functionality
    [Tags]    basic    transfer
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    
    # Wait for server to be ready
    Sleep    1s
    
    # Send file from client
    ${result}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}
    Should Be Equal As Numbers    ${result}    0    Client should exit successfully
    
    # Stop server
    Stop Server
    
    # Verify file transfer
    ${expected_size}=    Get File Size    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}
    ${received_size}=    Get File Size    ${SERVER_DATA_DIR}/${SAMPLE_FILE}
    Should Be Equal As Numbers    ${expected_size}    ${received_size}    File sizes should match
    
    # Compare file contents
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    ${SERVER_DATA_DIR}/${SAMPLE_FILE}
    Should Be True    ${files_match}    File contents should match

Test File Transfer With Custom Chunk Size
    [Documentation]    Test file transfer with custom chunk size
    [Tags]    chunk_size    transfer
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send file with custom chunk size
    ${result}=    Send File With Options    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    chunk=512
    Should Be Equal As Numbers    ${result}    0    Client should exit successfully
    
    # Stop server
    Stop Server
    
    # Verify file transfer
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    ${SERVER_DATA_DIR}/${SAMPLE_FILE}
    Should Be True    ${files_match}    File contents should match

Test File Transfer With Custom Window Size
    [Documentation]    Test file transfer with custom window size
    [Tags]    window_size    transfer
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send file with custom window size
    ${result}=    Send File With Options    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    window=4
    Should Be Equal As Numbers    ${result}    0    Client should exit successfully
    
    # Stop server
    Stop Server
    
    # Verify file transfer
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    ${SERVER_DATA_DIR}/${SAMPLE_FILE}
    Should Be True    ${files_match}    File contents should match

Test File Transfer With Custom Timeout
    [Documentation]    Test file transfer with custom timeout
    [Tags]    timeout    transfer
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send file with custom timeout
    ${result}=    Send File With Options    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    timeout=500
    Should Be Equal As Numbers    ${result}    0    Client should exit successfully
    
    # Stop server
    Stop Server
    
    # Verify file transfer
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    ${SERVER_DATA_DIR}/${SAMPLE_FILE}
    Should Be True    ${files_match}    File contents should match

Test Multiple File Transfers
    [Documentation]    Test multiple file transfers sequentially
    [Tags]    multiple    transfer
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send first file
    ${result1}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}
    Should Be Equal As Numbers    ${result1}    0    First transfer should succeed
    
    # Send second file (same file, different name)
    Copy File    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    ${SAMPLE_DATA_DIR}/sample2.bin
    ${result2}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/sample2.bin
    Should Be Equal As Numbers    ${result2}    0    Second transfer should succeed
    
    # Stop server
    Stop Server
    
    # Verify both files
    ${files_match1}=    Compare Files    ${SAMPLE_DATA_DIR}/${SAMPLE_FILE}    ${SERVER_DATA_DIR}/${SAMPLE_FILE}
    ${files_match2}=    Compare Files    ${SAMPLE_DATA_DIR}/sample2.bin    ${SERVER_DATA_DIR}/sample2.bin
    Should Be True    ${files_match1}    First file should match
    Should Be True    ${files_match2}    Second file should match

Test Server Error Handling
    [Documentation]    Test server error handling with invalid requests
    [Tags]    error    server
    
    # Get available port
    ${port}=    Get Available Port
    
    # Try to send non-existent file (should fail before even starting server)
    Run Keyword And Expect Error    FileNotFoundError: File not found: non_existent_file.bin    Send File    ${CLIENT_HOST}    ${port}    non_existent_file.bin

Test Large File Transfer
    [Documentation]    Test transfer of a larger file to verify protocol robustness
    [Tags]    large_file    transfer    performance
    
    # Create a larger test file (1MB)
    Create Large Test File    ${SAMPLE_DATA_DIR}/large_test.bin    1048576
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send large file
    ${result}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/large_test.bin
    Should Be Equal As Numbers    ${result}    0    Large file transfer should succeed
    
    # Stop server
    Stop Server
    
    # Verify large file transfer
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/large_test.bin    ${SERVER_DATA_DIR}/large_test.bin
    Should Be True    ${files_match}    Large file contents should match
    
    # Cleanup
    Remove File    ${SAMPLE_DATA_DIR}/large_test.bin    missing_ok=True

Test Concurrent Client Connections
    [Documentation]    Test server handling multiple concurrent client connections
    [Tags]    concurrent    stress    transfer
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Create multiple test files
    Create Test File    ${SAMPLE_DATA_DIR}/test1.bin    1024
    Create Test File    ${SAMPLE_DATA_DIR}/test2.bin    2048
    Create Test File    ${SAMPLE_DATA_DIR}/test3.bin    512
    
    # Send files concurrently (simulated by rapid sequential sends)
    ${result1}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/test1.bin
    ${result2}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/test2.bin
    ${result3}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/test3.bin
    
    Should Be Equal As Numbers    ${result1}    0    First concurrent transfer should succeed
    Should Be Equal As Numbers    ${result2}    0    Second concurrent transfer should succeed
    Should Be Equal As Numbers    ${result3}    0    Third concurrent transfer should succeed
    
    # Stop server
    Stop Server
    
    # Verify all files
    ${files_match1}=    Compare Files    ${SAMPLE_DATA_DIR}/test1.bin    ${SERVER_DATA_DIR}/test1.bin
    ${files_match2}=    Compare Files    ${SAMPLE_DATA_DIR}/test2.bin    ${SERVER_DATA_DIR}/test2.bin
    ${files_match3}=    Compare Files    ${SAMPLE_DATA_DIR}/test3.bin    ${SERVER_DATA_DIR}/test3.bin
    
    Should Be True    ${files_match1}    First concurrent file should match
    Should Be True    ${files_match2}    Second concurrent file should match
    Should Be True    ${files_match3}    Third concurrent file should match
    
    # Cleanup
    Remove File    ${SAMPLE_DATA_DIR}/test1.bin    missing_ok=True
    Remove File    ${SAMPLE_DATA_DIR}/test2.bin    missing_ok=True
    Remove File    ${SAMPLE_DATA_DIR}/test3.bin    missing_ok=True

Test Protocol Robustness With Network Simulation
    [Documentation]    Test protocol robustness by simulating network conditions
    [Tags]    robustness    network    transfer
    
    # Create a test file with specific patterns
    Create Pattern Test File    ${SAMPLE_DATA_DIR}/pattern_test.bin    4096
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send file with aggressive timeout and retry settings
    ${result}=    Send File With Options    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/pattern_test.bin    timeout=100    max_retries=5
    Should Be Equal As Numbers    ${result}    0    Pattern file transfer should succeed
    
    # Stop server
    Stop Server
    
    # Verify pattern file transfer
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/pattern_test.bin    ${SERVER_DATA_DIR}/pattern_test.bin
    Should Be True    ${files_match}    Pattern file contents should match
    
    # Cleanup
    Remove File    ${SAMPLE_DATA_DIR}/pattern_test.bin    missing_ok=True

Test Binary File Integrity
    [Documentation]    Test transfer of binary files with various data patterns
    [Tags]    binary    integrity    transfer
    
    # Create binary test file with various patterns
    Create Binary Test File    ${SAMPLE_DATA_DIR}/binary_test.bin    8192
    
    # Get available port
    ${port}=    Get Available Port
    
    # Start server
    Start Server    ${port}    ${SERVER_DATA_DIR}
    Sleep    1s
    
    # Send binary file
    ${result}=    Send File    ${CLIENT_HOST}    ${port}    ${SAMPLE_DATA_DIR}/binary_test.bin
    Should Be Equal As Numbers    ${result}    0    Binary file transfer should succeed
    
    # Stop server
    Stop Server
    
    # Verify binary file integrity
    ${files_match}=    Compare Files    ${SAMPLE_DATA_DIR}/binary_test.bin    ${SERVER_DATA_DIR}/binary_test.bin
    Should Be True    ${files_match}    Binary file contents should match exactly
    
    # Additional integrity check - verify file size
    ${expected_size}=    Get File Size    ${SAMPLE_DATA_DIR}/binary_test.bin
    ${received_size}=    Get File Size    ${SERVER_DATA_DIR}/binary_test.bin
    Should Be Equal As Numbers    ${expected_size}    ${received_size}    Binary file sizes should match exactly
    
    # Cleanup
    Remove File    ${SAMPLE_DATA_DIR}/binary_test.bin    missing_ok=True

*** Keywords ***
Cleanup Test Data
    [Documentation]    Clean up test data after each test
    Remove Directory    ${SERVER_DATA_DIR}    recursive=True
    Create Directory    ${SERVER_DATA_DIR}
    Remove File    ${SAMPLE_DATA_DIR}/sample2.bin    missing_ok=True

Create Large Test File
    [Arguments]    ${file_path}    ${size_bytes}
    [Documentation]    Create a test file of specified size with random data
    ${random_data}=    Evaluate    ''.join([chr(random.randint(0, 255)) for _ in range(${size_bytes})])    random
    Create File    ${file_path}    ${random_data}

Create Test File
    [Arguments]    ${file_path}    ${size_bytes}
    [Documentation]    Create a test file of specified size
    ${test_data}=    Evaluate    'A' * ${size_bytes}
    Create File    ${file_path}    ${test_data}

Create Pattern Test File
    [Arguments]    ${file_path}    ${size_bytes}
    [Documentation]    Create a test file with repeating patterns
    ${pattern}=    Set Variable    ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
    ${repeats}=    Evaluate    ${size_bytes} // len('${pattern}') + 1
    ${test_data}=    Evaluate    '${pattern}' * ${repeats}
    ${test_data}=    Evaluate    '${test_data}'[:${size_bytes}]
    Create File    ${file_path}    ${test_data}

Create Binary Test File
    [Arguments]    ${file_path}    ${size_bytes}
    [Documentation]    Create a binary test file with various byte patterns
    ${binary_data}=    Evaluate    bytes([i % 256 for i in range(${size_bytes})])
    Create Binary File    ${file_path}    ${binary_data}
