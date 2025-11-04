import os
import subprocess
import time
import signal
import shutil
from pathlib import Path
from typing import Optional, Dict, Any


class UdpTransferLibrary:
    """Robot Framework library for testing UDP reliable transfer protocol."""
    
    def __init__(self):
        self.server_process = None
        # Get the project root directory (2 levels up from this file)
        project_root = Path(__file__).parent.parent.parent
        self.build_dir = str(project_root / "build" / "bin")
        self.server_data_dir = str(project_root / "server_data")
        self.sample_data_dir = str(project_root / "sample_data")
    
    def start_server(self, port: int = 9000, output_dir: str = None) -> None:
        """Start the UDP server.
        
        Args:
            port: Server port number
            output_dir: Directory where received files will be saved
        """
        if self.server_process:
            self.stop_server()
        
        # Use default output directory if none provided
        if output_dir is None:
            output_dir = self.server_data_dir
            
        # Ensure output directory exists
        os.makedirs(output_dir, exist_ok=True)
        
        # Start server process
        server_cmd = [f"{self.build_dir}/server.exe", "--port", str(port), "--out", output_dir]
        self.server_process = subprocess.Popen(
            server_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Wait a bit for server to start
        time.sleep(0.5)
        
        if self.server_process.poll() is not None:
            raise RuntimeError(f"Server failed to start: {self.server_process.stderr.read()}")
    
    def stop_server(self) -> None:
        """Stop the UDP server."""
        if self.server_process:
            self.server_process.terminate()
            try:
                self.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server_process.kill()
                self.server_process.wait()
            self.server_process = None
    
    def send_file(self, host: str, port: int, file_path: str) -> int:
        """Send a file using the UDP client.
        
        Args:
            host: Server hostname/IP
            port: Server port
            file_path: Path to file to send
            
        Returns:
            Exit code of the client process
        """
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"File not found: {file_path}")
        
        client_cmd = [
            f"{self.build_dir}/client.exe",
            "--host", host,
            "--port", str(port),
            "--file", file_path
        ]
        
        result = subprocess.run(client_cmd, capture_output=True, text=True)
        return result.returncode
    
    def send_file_with_options(self, host: str, port: int, file_path: str, **options) -> int:
        """Send a file using the UDP client with custom options.
        
        Args:
            host: Server hostname/IP
            port: Server port
            file_path: Path to file to send
            **options: Additional options (chunk, window, timeout, max_retries)
            
        Returns:
            Exit code of the client process
        """
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"File not found: {file_path}")
        
        client_cmd = [
            f"{self.build_dir}/client.exe",
            "--host", host,
            "--port", str(port),
            "--file", file_path
        ]
        
        # Add optional parameters
        if 'chunk' in options:
            client_cmd.extend(["--chunk", str(options['chunk'])])
        if 'window' in options:
            client_cmd.extend(["--window", str(options['window'])])
        if 'timeout' in options:
            client_cmd.extend(["--timeout", str(options['timeout'])])
        if 'max_retries' in options:
            client_cmd.extend(["--max-retries", str(options['max_retries'])])
        
        result = subprocess.run(client_cmd, capture_output=True, text=True)
        return result.returncode
    
    def get_file_size(self, file_path: str) -> int:
        """Get the size of a file in bytes.
        
        Args:
            file_path: Path to the file
            
        Returns:
            File size in bytes
        """
        if not os.path.exists(file_path):
            return 0
        return os.path.getsize(file_path)
    
    def compare_files(self, file1: str, file2: str) -> bool:
        """Compare two files for equality.
        
        Args:
            file1: Path to first file
            file2: Path to second file
            
        Returns:
            True if files are identical, False otherwise
        """
        if not os.path.exists(file1) or not os.path.exists(file2):
            return False
        
        # Compare file sizes first
        if os.path.getsize(file1) != os.path.getsize(file2):
            return False
        
        # Compare file contents
        with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
            return f1.read() == f2.read()
    
    def copy_file(self, src: str, dst: str) -> None:
        """Copy a file from source to destination.
        
        Args:
            src: Source file path
            dst: Destination file path
        """
        shutil.copy2(src, dst)
    
    def remove_file(self, file_path: str, missing_ok: bool = False) -> None:
        """Remove a file.
        
        Args:
            file_path: Path to file to remove
            missing_ok: If True, don't raise error if file doesn't exist
        """
        try:
            os.remove(file_path)
        except FileNotFoundError:
            if not missing_ok:
                raise
    
    def create_directory(self, dir_path: str) -> None:
        """Create a directory.
        
        Args:
            dir_path: Path to directory to create
        """
        os.makedirs(dir_path, exist_ok=True)
    
    def remove_directory(self, dir_path: str, recursive: bool = False) -> None:
        """Remove a directory.
        
        Args:
            dir_path: Path to directory to remove
            recursive: If True, remove directory and all contents
        """
        if recursive:
            shutil.rmtree(dir_path, ignore_errors=True)
        else:
            os.rmdir(dir_path)
    
    def file_exists(self, file_path: str) -> bool:
        """Check if a file exists.
        
        Args:
            file_path: Path to file to check
            
        Returns:
            True if file exists, False otherwise
        """
        return os.path.exists(file_path)
    
    def directory_exists(self, dir_path: str) -> bool:
        """Check if a directory exists.
        
        Args:
            dir_path: Path to directory to check
            
        Returns:
            True if directory exists, False otherwise
        """
        return os.path.isdir(dir_path)
    
    def get_server_output(self) -> str:
        """Get the server's stdout output.
        
        Returns:
            Server's stdout output as string
        """
        if self.server_process:
            return self.server_process.stdout.read() if self.server_process.stdout else ""
        return ""
    
    def get_server_error(self) -> str:
        """Get the server's stderr output.
        
        Returns:
            Server's stderr output as string
        """
        if self.server_process:
            return self.server_process.stderr.read() if self.server_process.stderr else ""
        return ""
    
    def wait_for_server_ready(self, timeout: int = 10) -> bool:
        """Wait for server to be ready.
        
        Args:
            timeout: Timeout in seconds
            
        Returns:
            True if server is ready, False if timeout
        """
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.server_process and self.server_process.poll() is None:
                return True
            time.sleep(0.1)
        return False
