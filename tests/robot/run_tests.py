#!/usr/bin/env python3
"""
Robot Framework test runner for UDP reliable transfer protocol.
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path


def install_requirements():
    """Install Robot Framework requirements."""
    requirements_file = Path(__file__).parent / "requirements.txt"
    if requirements_file.exists():
        print("Installing Robot Framework requirements...")
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(requirements_file)], check=True)


def run_tests(test_file=None, output_dir=None, tags=None, exclude_tags=None):
    """Run Robot Framework tests.
    
    Args:
        test_file: Specific test file to run (optional)
        output_dir: Output directory for test results
        tags: Tags to include
        exclude_tags: Tags to exclude
    """
    # Change to the robot test directory
    robot_dir = Path(__file__).parent
    os.chdir(robot_dir)
    
    # Build the robot command
    cmd = ["robot"]
    
    if output_dir:
        cmd.extend(["--outputdir", output_dir])
    
    if tags:
        cmd.extend(["--include", tags])
    
    if exclude_tags:
        cmd.extend(["--exclude", exclude_tags])
    
    # Add test file or directory
    if test_file:
        cmd.append(test_file)
    else:
        cmd.append(".")
    
    print(f"Running Robot Framework tests with command: {' '.join(cmd)}")
    
    # Run the tests
    result = subprocess.run(cmd)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description="Run Robot Framework tests for UDP transfer")
    parser.add_argument("--test-file", help="Specific test file to run")
    parser.add_argument("--output-dir", help="Output directory for test results")
    parser.add_argument("--tags", help="Tags to include")
    parser.add_argument("--exclude-tags", help="Tags to exclude")
    parser.add_argument("--install-requirements", action="store_true", help="Install requirements before running tests")
    
    args = parser.parse_args()
    
    if args.install_requirements:
        install_requirements()
    
    return run_tests(
        test_file=args.test_file,
        output_dir=args.output_dir,
        tags=args.tags,
        exclude_tags=args.exclude_tags
    )


if __name__ == "__main__":
    sys.exit(main())
