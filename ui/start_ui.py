#!/usr/bin/env python3
"""
Startup script for UDP Transfer UI
Handles setup, dependency installation, and launching the UI
"""

import os
import sys
import subprocess
import webbrowser
import time
from pathlib import Path

def check_python_version():
    """Check if Python version is compatible"""
    if sys.version_info < (3, 7):
        print("Error: Python 3.7 or higher is required")
        print(f"Current version: {sys.version}")
        sys.exit(1)
    print(f"✓ Python version: {sys.version.split()[0]}")

def install_dependencies():
    """Install required Python dependencies"""
    requirements_file = Path(__file__).parent / "requirements.txt"
    
    if not requirements_file.exists():
        print("Error: requirements.txt not found")
        sys.exit(1)
    
    print("Installing Python dependencies...")
    try:
        subprocess.run([
            sys.executable, "-m", "pip", "install", "-r", str(requirements_file)
        ], check=True)
        print("✓ Dependencies installed successfully")
    except subprocess.CalledProcessError as e:
        print(f"Error installing dependencies: {e}")
        sys.exit(1)

def check_build():
    """Check if the project is built"""
    build_dir = Path(__file__).parent.parent / "build"
    server_exe = build_dir / "bin" / ("server.exe" if os.name == 'nt' else "server")
    client_exe = build_dir / "bin" / ("client.exe" if os.name == 'nt' else "client")
    
    if not build_dir.exists():
        print("Error: Build directory not found")
        print("Please build the project first:")
        print("  cmake -B build -S .")
        print("  cmake --build build")
        sys.exit(1)
    
    if not server_exe.exists():
        print(f"Error: Server executable not found at {server_exe}")
        print("Please build the project first")
        sys.exit(1)
    
    if not client_exe.exists():
        print(f"Error: Client executable not found at {client_exe}")
        print("Please build the project first")
        sys.exit(1)
    
    print("✓ Build artifacts found")

def create_directories():
    """Create necessary directories"""
    server_data_dir = Path(__file__).parent.parent / "server_data"
    server_data_dir.mkdir(exist_ok=True)
    print(f"✓ Server data directory: {server_data_dir}")

def start_backend():
    """Start the Flask backend server"""
    server_script = Path(__file__).parent / "server.py"
    
    print("Starting backend server...")
    try:
        # Start the server in a subprocess
        process = subprocess.Popen([
            sys.executable, str(server_script)
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        # Wait a moment for server to start
        time.sleep(2)
        
        # Check if server started successfully
        if process.poll() is None:
            print("✓ Backend server started successfully")
            return process
        else:
            stdout, stderr = process.communicate()
            print(f"Error starting backend server:")
            print(f"STDOUT: {stdout}")
            print(f"STDERR: {stderr}")
            sys.exit(1)
            
    except Exception as e:
        print(f"Error starting backend server: {e}")
        sys.exit(1)

def open_browser():
    """Open the UI in the default browser"""
    url = "http://localhost:5000"
    print(f"Opening UI in browser: {url}")
    
    try:
        webbrowser.open(url)
        print("✓ Browser opened successfully")
    except Exception as e:
        print(f"Warning: Could not open browser automatically: {e}")
        print(f"Please open {url} manually in your browser")

def main():
    """Main startup function"""
    print("UDP Transfer UI Startup")
    print("=" * 40)
    
    # Check Python version
    check_python_version()
    
    # Install dependencies
    install_dependencies()
    
    # Check build
    check_build()
    
    # Create directories
    create_directories()
    
    # Start backend
    backend_process = start_backend()
    
    # Open browser
    open_browser()
    
    print("\nUI is now running!")
    print("Press Ctrl+C to stop the server")
    print("=" * 40)
    
    try:
        # Keep the script running
        backend_process.wait()
    except KeyboardInterrupt:
        print("\nShutting down...")
        
        # Send SIGTERM to the Flask process
        backend_process.terminate()
        
        # Wait for graceful shutdown
        try:
            backend_process.wait(timeout=10)
            print("✓ Server stopped gracefully")
        except subprocess.TimeoutExpired:
            print("Force killing server...")
            backend_process.kill()
            try:
                backend_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
        
        # Force close port 5000 if still in use
        import socket
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(('localhost', 5000))
            sock.close()
            print("✓ Port 5000 released")
        except:
            pass
        
        print("Goodbye!")

if __name__ == "__main__":
    main()
