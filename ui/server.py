#!/usr/bin/env python3
"""
Flask backend server for UDP Transfer UI
Handles communication between the web UI and the UDP transfer executables
"""

import os
import sys
import json
import subprocess
import threading
import time
import signal
import socket
from datetime import datetime
from pathlib import Path
from flask import Flask, request, jsonify, send_file
from flask_cors import CORS
import psutil

app = Flask(__name__, static_folder=str(Path(__file__).parent), static_url_path='')
CORS(app)  # Enable CORS for all routes

# Configuration
# Check if we're running in Docker (executables in /app/) or locally (in build/bin/)
if Path("/app/server").exists():
    BUILD_DIR = Path("/app")
else:
    BUILD_DIR = Path(__file__).parent.parent / "build" / "bin"

SERVER_DATA_DIR = Path(__file__).parent.parent / "server_data"
SAMPLE_DATA_DIR = Path(__file__).parent.parent / "sample_data"

# Global state
server_process = None
server_port = None
server_output_dir = None

# Health check and monitoring
app_start_time = datetime.now()
request_count = 0
error_count = 0

class ServerManager:
    def __init__(self):
        self.process = None
        self.port = None
        self.output_dir = None
        self.is_running = False
        self.logs = []
        self.process_id = None  # Track the actual process ID
        self.start_time = None
        self.last_health_check = None
        
    def start_server(self, port, output_dir):
        """Start the UDP server"""
        if self.is_running:
            # Stop existing server first
            self.stop_server()
            
        try:
            # Ensure output directory exists
            Path(output_dir).mkdir(parents=True, exist_ok=True)
            
            # Build the command
            server_exe = BUILD_DIR / "server.exe" if os.name == 'nt' else BUILD_DIR / "server"
            
            # Check if executable exists
            if not server_exe.exists():
                return {"success": False, "error": f"Server executable not found at {server_exe}"}
            
            # Try to find an available port starting from the requested port
            original_port = port
            max_port_attempts = 10
            
            for attempt in range(max_port_attempts):
                try:
                    cmd = [str(server_exe), "--port", str(port), "--out", output_dir]
                    print(f"Attempting to start server on port {port} (attempt {attempt + 1}/{max_port_attempts})")
                    
                    # Start the process
                    self.process = subprocess.Popen(
                        cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                        bufsize=1,
                        universal_newlines=True
                    )
                    self.process_id = self.process.pid
                    
                    # Wait a moment to see if server starts successfully
                    time.sleep(3)  # Increased wait time for better stability
                    
                    # Check if process is still running
                    if self.process.poll() is not None:
                        # Server failed to start, try next port
                        error_output = self.process.stderr.read() if self.process.stderr else "Unknown error"
                        print(f"Server failed to start on port {port}: {error_output}")
                        if "bind failed" in error_output or "10048" in error_output or "address already in use" in error_output.lower():
                            # Port is in use, try next port
                            port += 1
                            continue
                        else:
                            # Other error, return it
                            self.is_running = False
                            return {"success": False, "error": f"Server failed to start: {error_output}"}
                    else:
                        # Server started successfully
                        self.port = port
                        self.output_dir = output_dir
                        self.is_running = True
                        self.start_time = datetime.now()
                        
                        # Start log monitoring thread
                        threading.Thread(target=self._monitor_logs, daemon=True).start()
                        
                        if port != original_port:
                            return {"success": True, "message": f"Server started on port {port} (original port {original_port} was in use)"}
                        else:
                            return {"success": True, "message": f"Server started on port {port}"}
                            
                except Exception as e:
                    if attempt < max_port_attempts - 1:
                        port += 1
                        continue
                    else:
                        self.is_running = False
                        return {"success": False, "error": str(e)}
            
            # If we get here, all ports were tried
            self.is_running = False
            return {"success": False, "error": f"Could not find available port in range {original_port}-{original_port + max_port_attempts - 1}"}
            
        except Exception as e:
            self.is_running = False
            return {"success": False, "error": str(e)}
    
    def stop_server(self):
        """Stop the UDP server"""
        if not self.is_running and not self.process:
            return {"success": True, "message": "Server is not running"}
            
        try:
            if self.process:
                # Try graceful shutdown first
                self.process.terminate()
                
                # Wait for graceful shutdown
                try:
                    self.process.wait(timeout=10)  # Increased timeout
                except subprocess.TimeoutExpired:
                    # Force kill if graceful shutdown fails
                    print(f"Force killing server process {self.process.pid}")
                    self.process.kill()
                    try:
                        self.process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        # If still not dead, try to kill the process tree
                        try:
                            import psutil
                            parent = psutil.Process(self.process.pid)
                            for child in parent.children(recursive=True):
                                child.kill()
                            parent.kill()
                        except:
                            pass
                
                self.process = None
                self.process_id = None
            
            self.is_running = False
            self.port = None
            self.output_dir = None
            self.start_time = None
            
            # Clear logs
            self.logs = []
            
            return {"success": True, "message": "Server stopped successfully"}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def _monitor_logs(self):
        """Monitor server logs in a separate thread"""
        if not self.process or not self.process.stdout:
            return
            
        try:
            for line in iter(self.process.stdout.readline, ''):
                if line:
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    log_entry = f"[{timestamp}] {line.strip()}"
                    self.logs.append(log_entry)
                    
                    # Keep only last 100 log entries
                    if len(self.logs) > 100:
                        self.logs = self.logs[-100:]
        except:
            pass
    
    def get_logs(self):
        """Get server logs"""
        return self.logs.copy()
    
    def get_status(self):
        """Get server status"""
        status = {
            "is_running": self.is_running,
            "port": self.port,
            "output_dir": self.output_dir,
            "pid": self.process.pid if self.process else None,
            "start_time": self.start_time.isoformat() if self.start_time else None,
            "uptime_seconds": (datetime.now() - self.start_time).total_seconds() if self.start_time else None
        }
        
        # Check if the process is actually still running
        if status.get('is_running') and status.get('pid'):
            try:
                # Check if process is still alive
                import psutil
                process = psutil.Process(status['pid'])
                if not process.is_running():
                    # Process died, update status
                    self.is_running = False
                    self.process = None
                    self.process_id = None
                    status = self.get_status()
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                # Process is dead or inaccessible
                self.is_running = False
                self.process = None
                self.process_id = None
                status = self.get_status()
        
        return status

# Global server manager instance
server_manager = ServerManager()

# Helper functions for file operations
def find_file_by_base_name(base_filename, output_dir):
    """Find a file that starts with the base filename (handles UDP server naming pattern)"""
    base_filename = base_filename.strip()
    if not base_filename:
        return None
    
    for file_path in Path(output_dir).iterdir():
        if file_path.is_file() and file_path.name.startswith(base_filename + '_'):
            return file_path
    return None

def get_base_filename(full_filename):
    """Extract the base filename from a UDP server filename (removes timestamp and IP suffix)"""
    if '_' not in full_filename:
        return full_filename
    
    # Find the last underscore followed by numbers (timestamp)
    parts = full_filename.split('_')
    if len(parts) >= 3:
        # Check if the last two parts look like timestamp and IP
        try:
            int(parts[-2])  # timestamp should be numeric
            # If we can parse the timestamp, return everything before it
            return '_'.join(parts[:-2])
        except ValueError:
            pass
    
    return full_filename

# Request tracking middleware
@app.before_request
def before_request():
    global request_count
    request_count += 1

@app.after_request
def after_request(response):
    global error_count
    if response.status_code >= 400:
        error_count += 1
    return response

# Health check endpoints
@app.route('/health', methods=['GET'])
def health_check():
    """Basic health check endpoint"""
    global app_start_time, request_count, error_count
    
    try:
        # Check if server is responsive
        uptime = (datetime.now() - app_start_time).total_seconds()
        
        health_status = {
            "status": "healthy",
            "timestamp": datetime.now().isoformat(),
            "uptime_seconds": uptime,
            "request_count": request_count,
            "error_count": error_count,
            "error_rate": (error_count / request_count * 100) if request_count > 0 else 0,
            "server_status": server_manager.get_status()
        }
        
        return jsonify(health_status), 200
    except Exception as e:
        return jsonify({
            "status": "unhealthy",
            "error": str(e),
            "timestamp": datetime.now().isoformat()
        }), 500

@app.route('/health/detailed', methods=['GET'])
def detailed_health_check():
    """Detailed health check with system information"""
    try:
        import psutil
        
        # System information
        cpu_percent = psutil.cpu_percent(interval=1)
        memory = psutil.virtual_memory()
        
        # Disk usage - handle Windows paths properly
        try:
            if os.name == 'nt':
                # On Windows, try C: drive first, then current directory
                try:
                    disk = psutil.disk_usage('C:\\')
                except:
                    disk = psutil.disk_usage('.')
            else:
                disk = psutil.disk_usage('/')
        except Exception as disk_error:
            # Create a mock disk object if all else fails
            class MockDisk:
                def __init__(self):
                    self.percent = 0
                    self.free = 1024 * 1024 * 1024  # 1GB
            disk = MockDisk()
        
        # Network connectivity test
        network_status = "unknown"
        try:
            # Test localhost connectivity
            socket.create_connection(("127.0.0.1", 5000), timeout=5)
            network_status = "connected"
        except:
            network_status = "disconnected"
        
        detailed_status = {
            "status": "healthy",
            "timestamp": datetime.now().isoformat(),
            "system": {
                "cpu_percent": cpu_percent,
                "memory_percent": memory.percent,
                "memory_available_mb": memory.available / (1024 * 1024),
                "disk_percent": disk.percent,
                "disk_free_gb": disk.free / (1024 * 1024 * 1024)
            },
            "network": {
                "status": network_status,
                "localhost_5000": network_status
            },
            "server_status": server_manager.get_status(),
            "build_artifacts": {
                "server_exe_exists": (BUILD_DIR / "server.exe").exists() if os.name == 'nt' else (BUILD_DIR / "server").exists(),
                "client_exe_exists": (BUILD_DIR / "client.exe").exists() if os.name == 'nt' else (BUILD_DIR / "client").exists(),
                "build_dir_exists": BUILD_DIR.exists()
            }
        }
        
        return jsonify(detailed_status), 200
    except Exception as e:
        return jsonify({
            "status": "unhealthy",
            "error": str(e),
            "timestamp": datetime.now().isoformat()
        }), 500

@app.route('/')
def index():
    """Serve the main UI page"""
    return app.send_static_file('index.html')

@app.route('/api/server/start', methods=['POST'])
def start_server():
    """Start the UDP server"""
    try:
        data = request.get_json() or {}
        port = data.get('port', 9000)
        output_dir = data.get('output_dir', str(SERVER_DATA_DIR))
        
        result = server_manager.start_server(port, output_dir)
        return jsonify(result)
    except Exception as e:
        return jsonify({"success": False, "error": f"Failed to start server: {str(e)}"}), 500

@app.route('/api/server/stop', methods=['POST'])
def stop_server():
    """Stop the UDP server"""
    try:
        result = server_manager.stop_server()
        return jsonify(result)
    except Exception as e:
        return jsonify({"success": False, "error": f"Failed to stop server: {str(e)}"}), 500

@app.route('/api/server/status', methods=['GET'])
def get_server_status():
    """Get server status"""
    try:
        status = server_manager.get_status()
        return jsonify(status)
    except Exception as e:
        return jsonify({"error": f"Failed to get server status: {str(e)}"}), 500

@app.route('/api/server/logs', methods=['GET'])
def get_server_logs():
    """Get server logs"""
    try:
        return jsonify({"logs": server_manager.get_logs()})
    except Exception as e:
        return jsonify({"error": f"Failed to get server logs: {str(e)}"}), 500

@app.route('/api/transfer/send', methods=['POST'])
def send_file():
    """Send a file using the UDP client"""
    try:
        print(f"File upload request received. Server running: {server_manager.is_running}")
        print(f"Request files: {list(request.files.keys())}")
        print(f"Request form data: {dict(request.form)}")
        
        # Check server status with more detailed logging
        server_status = server_manager.get_status()
        print(f"Server status: {server_status}")
        
        if not server_manager.is_running:
            print("Server is not running, attempting to start it automatically...")
            # Try to start the server automatically
            start_result = server_manager.start_server(9000, str(SERVER_DATA_DIR))
            if not start_result.get('success'):
                return jsonify({
                    "success": False, 
                    "error": f"Server is not running and failed to start: {start_result.get('error', 'Unknown error')}"
                }), 400
            print("Server started automatically")
        
        # Handle file upload with better validation
        if 'file' not in request.files:
            print("No file in request.files")
            return jsonify({"success": False, "error": "No file provided in request"}), 400
        
        uploaded_file = request.files['file']
        if uploaded_file.filename == '':
            print("Empty filename")
            return jsonify({"success": False, "error": "No file selected"}), 400
        
        print(f"Processing file: {uploaded_file.filename}")
        
        # Save uploaded file to temporary location with original filename
        import tempfile
        import shutil
        
        # Create a temporary directory to preserve the original filename
        temp_dir = tempfile.mkdtemp()
        original_filename = uploaded_file.filename
        temp_file_path = os.path.join(temp_dir, original_filename)
        
        # Save the uploaded file with its original name
        uploaded_file.save(temp_file_path)
        print(f"File saved to: {temp_file_path}")
        
        try:
            # Get parameters from form data with better validation
            host = request.form.get('host', '127.0.0.1')
            # Use the actual server port that's running
            port = server_manager.port if server_manager.port else 9000
            
            # Validate and convert parameters with error handling
            try:
                chunk_size = int(request.form.get('chunk_size', 1024))
                if chunk_size < 64 or chunk_size > 8192:
                    return jsonify({"success": False, "error": f"Invalid chunk_size: {chunk_size}. Must be between 64 and 8192"}), 400
            except ValueError:
                return jsonify({"success": False, "error": "Invalid chunk_size parameter"}), 400
            
            try:
                window_size = int(request.form.get('window_size', 8))
                if window_size < 1 or window_size > 32:
                    return jsonify({"success": False, "error": f"Invalid window_size: {window_size}. Must be between 1 and 32"}), 400
            except ValueError:
                return jsonify({"success": False, "error": "Invalid window_size parameter"}), 400
            
            try:
                timeout = int(request.form.get('timeout', 300))
                if timeout < 100 or timeout > 5000:
                    return jsonify({"success": False, "error": f"Invalid timeout: {timeout}. Must be between 100 and 5000"}), 400
            except ValueError:
                return jsonify({"success": False, "error": "Invalid timeout parameter"}), 400
            
            try:
                max_retries = int(request.form.get('max_retries', 3))
                if max_retries < 1 or max_retries > 10:
                    return jsonify({"success": False, "error": f"Invalid max_retries: {max_retries}. Must be between 1 and 10"}), 400
            except ValueError:
                return jsonify({"success": False, "error": "Invalid max_retries parameter"}), 400
            
            print(f"Transfer parameters: host={host}, port={port}, chunk_size={chunk_size}, window_size={window_size}, timeout={timeout}, max_retries={max_retries}")
            
            # Build the command
            client_exe = BUILD_DIR / "client.exe" if os.name == 'nt' else BUILD_DIR / "client"
            
            if not client_exe.exists():
                return jsonify({"success": False, "error": f"Client executable not found at {client_exe}"}), 500
            
            cmd = [
                str(client_exe),
                "--host", host,
                "--port", str(port),
                "--file", temp_file_path,
                "--chunk", str(chunk_size),
                "--window", str(window_size),
                "--timeout", str(timeout),
                "--max-retries", str(max_retries)
            ]
            
            # Run the client with longer timeout for large files
            start_time = time.time()
            print(f"Running command: {' '.join(cmd)}")
            
            # Calculate timeout based on file size: 1 minute per MB + 2 minutes base
            file_size_mb = os.path.getsize(temp_file_path) / (1024 * 1024)
            calculated_timeout = max(300, int(file_size_mb * 60 + 120))  # At least 5 minutes, more for larger files
            print(f"Using timeout: {calculated_timeout} seconds for {file_size_mb:.2f} MB file")
            
            # Final check if server is still running before starting transfer
            if not server_manager.is_running:
                return jsonify({"success": False, "error": "Server stopped running before transfer could begin"}), 400
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=calculated_timeout)
            end_time = time.time()
            
            print(f"Client return code: {result.returncode}")
            print(f"Client stdout: {result.stdout}")
            print(f"Client stderr: {result.stderr}")
            
            if result.returncode == 0:
                duration = end_time - start_time
                file_size = os.path.getsize(temp_file_path)
                speed = file_size / duration / 1024  # KB/s
                
                # Wait a moment for file to be written
                time.sleep(2)  # Increased wait time for debugging
                
                return jsonify({
                    "success": True,
                    "message": "File transferred successfully",
                    "duration": duration,
                    "speed": speed,
                    "file_size": file_size
                })
            else:
                error_msg = result.stderr if result.stderr else "Unknown transfer error"
                return jsonify({
                    "success": False,
                    "error": f"Transfer failed: {error_msg}"
                }), 500
                
        finally:
            # Clean up temporary directory and file
            try:
                shutil.rmtree(temp_dir)
                print(f"Cleaned up temporary directory: {temp_dir}")
            except Exception as cleanup_error:
                print(f"Warning: Failed to clean up temporary directory {temp_dir}: {cleanup_error}")
                
    except subprocess.TimeoutExpired:
        return jsonify({"success": False, "error": "Transfer timed out - the file may be too large or the server may be busy"}), 408
    except Exception as e:
        print(f"Unexpected error in send_file: {str(e)}")
        return jsonify({"success": False, "error": f"Transfer error: {str(e)}"}), 500

@app.route('/api/files/list', methods=['GET'])
def list_files():
    """List files in the server output directory"""
    try:
        # Always use the default server data directory for listing files
        output_dir = SERVER_DATA_DIR
        files = []
        
        print(f"Listing files from: {output_dir}")
        
        if not os.path.exists(output_dir):
            print(f"Output directory does not exist: {output_dir}")
            return jsonify({"files": [], "message": "Output directory does not exist"})
        
        for file_path in Path(output_dir).iterdir():
            if file_path.is_file():
                try:
                    stat = file_path.stat()
                    # Include empty files for debugging (temporarily)
                    if stat.st_size == 0:
                        print(f"Found empty file: {file_path.name} (including for debugging)")
                        # Don't skip empty files for now to debug the issue
                    base_name = get_base_filename(file_path.name)
                    files.append({
                        "name": file_path.name,
                        "base_name": base_name,
                        "size": stat.st_size,
                        "modified": datetime.fromtimestamp(stat.st_mtime).isoformat(),
                        "path": str(file_path)
                    })
                except Exception as file_error:
                    print(f"Error reading file {file_path}: {file_error}")
                    continue
        
        # Sort files by modification time (most recent first)
        files.sort(key=lambda x: x['modified'], reverse=True)
        
        print(f"Found {len(files)} non-empty files")
        return jsonify({"files": files, "success": True})
        
    except Exception as e:
        print(f"Error listing files: {e}")
        return jsonify({"success": False, "error": str(e), "files": []}), 500

@app.route('/api/files/download/<filename>', methods=['GET'])
def download_file(filename):
    """Download a file from the server output directory"""
    try:
        # Always use the default server data directory for downloads
        output_dir = SERVER_DATA_DIR
        file_path = Path(output_dir) / filename
        
        print(f"Download request for: {filename}")
        print(f"Output dir: {output_dir}")
        print(f"File path: {file_path}")
        print(f"File exists: {file_path.exists()}")
        print(f"File size: {file_path.stat().st_size if file_path.exists() else 'N/A'}")
        print(f"Current working directory: {os.getcwd()}")
        
        if not file_path.exists():
            # If exact match not found, try to find by base filename (UDP server naming pattern)
            print(f"Exact file not found, searching by base name: {filename}")
            file_path = find_file_by_base_name(filename, output_dir)
            
            if file_path is None:
                print(f"File not found by base name: {filename}")
                return jsonify({"success": False, "error": f"File '{filename}' not found"}), 404
            else:
                print(f"Found file by base name: {file_path}")
        
        # Check if it's actually a file
        if not file_path.is_file():
            print(f"Path is not a file: {file_path}")
            return jsonify({"success": False, "error": f"'{filename}' is not a file"}), 400
        
        print(f"Sending file: {file_path}")
        # Try different approaches for Flask compatibility
        try:
            # First try with download_name (newer Flask versions)
            return send_file(
                file_path, 
                as_attachment=True, 
                download_name=filename,
                mimetype='application/octet-stream'
            )
        except TypeError:
            try:
                # Fallback to attachment_filename (older Flask versions)
                return send_file(
                    file_path, 
                    as_attachment=True, 
                    attachment_filename=filename,
                    mimetype='application/octet-stream'
                )
            except TypeError:
                try:
                    # Another fallback without filename
                    return send_file(
                        file_path, 
                        as_attachment=True,
                        mimetype='application/octet-stream'
                    )
                except Exception as e:
                    # Last resort: return file content directly
                    with open(file_path, 'rb') as f:
                        content = f.read()
                    response = app.response_class(content, mimetype='application/octet-stream')
                    response.headers['Content-Disposition'] = f'attachment; filename="{filename}"'
                    return response
        
    except Exception as e:
        print(f"Download error: {str(e)}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/files/delete/<filename>', methods=['DELETE'])
def delete_file(filename):
    """Delete a file from the server output directory"""
    try:
        # Always use the default server data directory for consistency
        output_dir = SERVER_DATA_DIR
        
        # First try to find the exact filename
        file_path = Path(output_dir) / filename
        
        print(f"Attempting to delete: {file_path}")
        
        if not file_path.exists():
            # If exact match not found, try to find by base filename (UDP server naming pattern)
            print(f"Exact file not found, searching by base name: {filename}")
            file_path = find_file_by_base_name(filename, output_dir)
            
            if file_path is None:
                print(f"File not found by base name: {filename}")
                return jsonify({"success": False, "error": "File not found"}), 404
            else:
                print(f"Found file by base name: {file_path}")
        
        file_path.unlink()
        print(f"Successfully deleted: {file_path}")
        return jsonify({"success": True, "message": "File deleted successfully"})
        
    except Exception as e:
        print(f"Error deleting file {filename}: {e}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/files/cleanup', methods=['POST'])
def cleanup_empty_files():
    """Clean up empty files from the server output directory"""
    try:
        output_dir = SERVER_DATA_DIR
        cleaned_count = 0
        
        if os.path.exists(output_dir):
            for file_path in Path(output_dir).iterdir():
                if file_path.is_file():
                    try:
                        stat = file_path.stat()
                        if stat.st_size == 0:
                            file_path.unlink()
                            cleaned_count += 1
                            print(f"Cleaned up empty file: {file_path.name}")
                    except Exception as file_error:
                        print(f"Error processing file {file_path}: {file_error}")
                        continue
        
        return jsonify({
            "success": True, 
            "message": f"Cleanup completed. Removed {cleaned_count} empty files."
        })
        
    except Exception as e:
        print(f"Error during cleanup: {e}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/debug/upload-test', methods=['POST'])
def debug_upload_test():
    """Debug endpoint to test file upload without actual transfer"""
    try:
        print(f"Debug upload test - Request files: {list(request.files.keys())}")
        print(f"Debug upload test - Request form: {dict(request.form)}")
        
        if 'file' not in request.files:
            return jsonify({"success": False, "error": "No file in request"}), 400
        
        uploaded_file = request.files['file']
        if uploaded_file.filename == '':
            return jsonify({"success": False, "error": "Empty filename"}), 400
        
        # Just return success without doing the transfer
        return jsonify({
            "success": True,
            "message": "File upload test successful",
            "filename": uploaded_file.filename,
            "content_type": uploaded_file.content_type,
            "server_running": server_manager.is_running,
            "server_status": server_manager.get_status()
        })
    except Exception as e:
        return jsonify({"success": False, "error": f"Debug test failed: {str(e)}"}), 500

# Error handlers
@app.errorhandler(404)
def not_found(error):
    return jsonify({"error": "Endpoint not found"}), 404

@app.errorhandler(500)
def internal_error(error):
    return jsonify({"error": "Internal server error"}), 500

@app.errorhandler(Exception)
def handle_exception(e):
    return jsonify({"error": f"Unhandled exception: {str(e)}"}), 500

if __name__ == '__main__':
    # Configure Flask for better performance and reliability
    app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max file size
    app.config['SEND_FILE_MAX_AGE_DEFAULT'] = 0  # Disable caching for development
    
    # Run with improved settings
    app.run(
        host='0.0.0.0',  # Allow external connections for testing
        port=5000,
        debug=False,  # Disable debug mode for production-like testing
        threaded=True,  # Enable threading for better concurrency
        use_reloader=False  # Disable reloader to avoid duplicate processes
    )
