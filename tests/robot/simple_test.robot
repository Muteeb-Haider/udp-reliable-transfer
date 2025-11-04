*** Settings ***
Documentation     Simple test to verify Robot Framework setup
Library           OperatingSystem

*** Test Cases ***
Test Robot Framework Setup
    [Documentation]    Verify that Robot Framework is working
    Should Be Equal    ${1}    ${1}    Basic assertion should work
    Log    Robot Framework is working correctly
