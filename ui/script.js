// UDP Transfer UI JavaScript
class UDPTransferUI {
    constructor() {
        this.serverProcess = null;
        this.serverRunning = false;
        this.transferInProgress = false;
        this.stats = {
            totalTransfers: 0,
            totalBytes: 0,
            successfulTransfers: 0,
            failedTransfers: 0,
            transferTimes: []
        };
        
        this.initializeEventListeners();
        this.loadStats();
        this.refreshFileList();
    }

    initializeEventListeners() {
        // Server controls
        document.getElementById('startServer').addEventListener('click', () => this.startServer());
        document.getElementById('stopServer').addEventListener('click', () => this.stopServer());
        
        // Client controls
        document.getElementById('sendFile').addEventListener('click', () => this.sendFile());
        document.getElementById('clearLog').addEventListener('click', () => this.clearLogs());
        document.getElementById('refreshFiles').addEventListener('click', () => this.refreshFileList());
        document.getElementById('cleanupFiles').addEventListener('click', () => this.cleanupEmptyFiles());
        
        // File input change
        document.getElementById('fileInput').addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (file) {
                document.getElementById('sendFile').disabled = false;
                this.showToast(`File selected: ${file.name} (${this.formatFileSize(file.size)})`, 'info');
            }
        });

        // Port synchronization
        document.getElementById('serverPort').addEventListener('change', (e) => {
            document.getElementById('clientPort').value = e.target.value;
        });

        document.getElementById('clientPort').addEventListener('change', (e) => {
            document.getElementById('serverPort').value = e.target.value;
        });
    }

    async startServer() {
        if (this.serverRunning) {
            this.showToast('Server is already running', 'warning');
            return;
        }

        const port = document.getElementById('serverPort').value;
        const outputDir = document.getElementById('serverOutputDir').value;

        this.updateServerStatus('starting');
        this.logToServer(`Starting server on port ${port}...`);

        try {
            const response = await fetch('/api/server/start', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    port: parseInt(port),
                    output_dir: outputDir
                })
            });

            const result = await response.json();
            
            if (result.success) {
                this.serverRunning = true;
                this.updateServerStatus('online');
                this.logToServer(`Server started successfully on port ${port}`);
                this.showToast('Server started successfully', 'success');
                
                // Update the port field if server started on a different port
                if (result.message && result.message.includes('original port')) {
                    // Extract the actual port from the message
                    const portMatch = result.message.match(/Server started on port (\d+)/);
                    if (portMatch) {
                        const actualPort = portMatch[1];
                        document.getElementById('serverPort').value = actualPort;
                        document.getElementById('clientPort').value = actualPort;
                        this.logToServer(`Port updated to ${actualPort} (original port was in use)`);
                    }
                }
                
                document.getElementById('startServer').disabled = true;
                document.getElementById('stopServer').disabled = false;
                
                // Start polling for logs
                this.startLogPolling();
            } else {
                this.updateServerStatus('offline');
                this.logToServer(`Failed to start server: ${result.error}`);
                this.showToast('Failed to start server', 'error');
            }
            
        } catch (error) {
            this.updateServerStatus('offline');
            this.logToServer(`Failed to start server: ${error.message}`);
            this.showToast('Failed to start server', 'error');
        }
    }

    async stopServer() {
        if (!this.serverRunning) {
            this.showToast('Server is not running', 'warning');
            return;
        }

        this.logToServer('Stopping server...');

        try {
            const response = await fetch('/api/server/stop', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                }
            });

            const result = await response.json();
            
            if (result.success) {
                this.serverRunning = false;
                this.updateServerStatus('offline');
                this.logToServer('Server stopped successfully');
                this.showToast('Server stopped successfully', 'success');
                
                document.getElementById('startServer').disabled = false;
                document.getElementById('stopServer').disabled = true;
                
                // Stop log polling
                this.stopLogPolling();
            } else {
                this.logToServer(`Failed to stop server: ${result.error}`);
                this.showToast('Failed to stop server', 'error');
            }
            
        } catch (error) {
            this.logToServer(`Failed to stop server: ${error.message}`);
            this.showToast('Failed to stop server', 'error');
        }
    }

    async sendFile() {
        if (!this.serverRunning) {
            this.showToast('Server is not running', 'error');
            return;
        }

        const fileInput = document.getElementById('fileInput');
        const file = fileInput.files[0];
        
        if (!file) {
            this.showToast('Please select a file first', 'warning');
            return;
        }

        if (this.transferInProgress) {
            this.showToast('Transfer already in progress', 'warning');
            return;
        }

        const host = document.getElementById('clientHost').value;
        const port = document.getElementById('clientPort').value;
        const chunkSize = document.getElementById('chunkSize').value;
        const windowSize = document.getElementById('windowSize').value;
        const timeout = document.getElementById('timeout').value;
        const maxRetries = document.getElementById('maxRetries').value;

        this.transferInProgress = true;
        this.showTransferProgress();
        
        const startTime = Date.now();
        this.logToClient(`Starting transfer of ${file.name} (${this.formatFileSize(file.size)})`);
        this.logToClient(`Parameters: chunk=${chunkSize}, window=${windowSize}, timeout=${timeout}ms, retries=${maxRetries}`);

        try {
            // Create FormData for file upload
            const formData = new FormData();
            formData.append('file', file);
            formData.append('host', host);
            formData.append('port', port);
            formData.append('chunk_size', chunkSize);
            formData.append('window_size', windowSize);
            formData.append('timeout', timeout);
            formData.append('max_retries', maxRetries);

            // Simulate progress updates during transfer
            const progressInterval = setInterval(() => {
                const currentProgress = Math.min(90, Math.random() * 100);
                this.updateTransferProgress(currentProgress);
            }, 500);

            const response = await fetch('/api/transfer/send', {
                method: 'POST',
                body: formData
            });

            clearInterval(progressInterval);
            this.updateTransferProgress(100);

            const result = await response.json();
            
            if (result.success) {
                const endTime = Date.now();
                const duration = (endTime - startTime) / 1000;
                const speed = file.size / duration / 1024; // KB/s

                this.stats.totalTransfers++;
                this.stats.totalBytes += file.size;
                this.stats.successfulTransfers++;
                this.stats.transferTimes.push(duration);

                this.logToClient(`Transfer completed successfully in ${duration.toFixed(2)}s (${speed.toFixed(2)} KB/s)`);
                this.showToast('File transferred successfully', 'success');
                
                this.updateStats();
                // Refresh file list after successful transfer
                setTimeout(() => this.refreshFileList(), 1000);
            } else {
                this.stats.totalTransfers++;
                this.stats.failedTransfers++;
                
                this.logToClient(`Transfer failed: ${result.error}`);
                this.showToast('File transfer failed', 'error');
                this.updateStats();
            }
            
        } catch (error) {
            this.stats.totalTransfers++;
            this.stats.failedTransfers++;
            
            this.logToClient(`Transfer failed: ${error.message}`);
            this.showToast('File transfer failed', 'error');
            this.updateStats();
        } finally {
            this.transferInProgress = false;
            this.hideTransferProgress();
            fileInput.value = '';
            document.getElementById('sendFile').disabled = true;
        }
    }



    // UI Update Methods
    updateServerStatus(status) {
        const statusDot = document.querySelector('#serverStatus .status-dot');
        const statusText = document.querySelector('#serverStatus .status-text');
        
        statusDot.className = `status-dot ${status}`;
        
        switch (status) {
            case 'online':
                statusText.textContent = 'Online';
                break;
            case 'offline':
                statusText.textContent = 'Offline';
                break;
            case 'starting':
                statusText.textContent = 'Starting...';
                break;
        }
    }

    showTransferProgress() {
        document.getElementById('transferProgress').style.display = 'block';
        this.updateTransferProgress(0);
    }

    hideTransferProgress() {
        document.getElementById('transferProgress').style.display = 'none';
    }

    updateTransferProgress(percentage) {
        document.getElementById('progressFill').style.width = `${percentage}%`;
        document.getElementById('progressText').textContent = `${percentage.toFixed(1)}%`;
    }

    logToServer(message) {
        const logContainer = document.getElementById('serverLog');
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = document.createElement('div');
        logEntry.textContent = `[${timestamp}] ${message}`;
        logContainer.appendChild(logEntry);
        logContainer.scrollTop = logContainer.scrollHeight;
    }

    logToClient(message) {
        const logContainer = document.getElementById('clientLog');
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = document.createElement('div');
        logEntry.textContent = `[${timestamp}] ${message}`;
        logContainer.appendChild(logEntry);
        logContainer.scrollTop = logContainer.scrollHeight;
    }

    clearLogs() {
        document.getElementById('serverLog').innerHTML = '';
        document.getElementById('clientLog').innerHTML = '';
        this.showToast('Logs cleared', 'info');
    }

    async refreshFileList() {
        const fileList = document.getElementById('fileList');
        fileList.innerHTML = '<div class="loading">Loading files...</div>';

        try {
            console.log('Refreshing file list...');
            const response = await fetch('/api/files/list');
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const result = await response.json();
            console.log('File list response:', result);
            
            if (result.files && result.files.length > 0) {
                fileList.innerHTML = '';
                result.files.forEach(file => {
                    const fileItem = this.createFileItem(file);
                    fileList.appendChild(fileItem);
                });
            } else {
                fileList.innerHTML = '<div style="text-align: center; color: #666; padding: 20px;">No files received yet</div>';
            }
            
        } catch (error) {
            console.error('Error refreshing file list:', error);
            fileList.innerHTML = '<div style="text-align: center; color: #f44336; padding: 20px;">Failed to load files: ' + error.message + '</div>';
        }
    }

    async cleanupEmptyFiles() {
        try {
            console.log('Cleaning up empty files...');
            const response = await fetch('/api/files/cleanup', {
                method: 'POST'
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const result = await response.json();
            console.log('Cleanup response:', result);
            
            if (result.success) {
                this.showToast(`Cleaned up ${result.cleaned_count} empty files`, 'success');
                // Refresh the file list after cleanup
                await this.refreshFileList();
            } else {
                this.showToast(`Cleanup failed: ${result.error}`, 'error');
            }
        } catch (error) {
            console.error('Error cleaning up files:', error);
            this.showToast(`Cleanup failed: ${error.message}`, 'error');
        }
    }

    createFileItem(file) {
        const fileItem = document.createElement('div');
        fileItem.className = 'file-item';
        
        const modifiedDate = new Date(file.modified).toLocaleString();
        
        fileItem.innerHTML = `
            <div class="file-info">
                <i class="fas fa-file file-icon"></i>
                <div class="file-details">
                    <h5>${file.name}</h5>
                    <p>${this.formatFileSize(file.size)} • Modified ${modifiedDate}</p>
                </div>
            </div>
            <div class="file-actions">
                <button class="btn btn-secondary" onclick="ui.downloadFile('${file.name}')">
                    <i class="fas fa-download"></i>
                </button>
                <button class="btn btn-danger" onclick="ui.deleteFile('${file.name}')">
                    <i class="fas fa-trash"></i>
                </button>
            </div>
        `;
        
        return fileItem;
    }

    startLogPolling() {
        this.logPollingInterval = setInterval(async () => {
            if (this.serverRunning) {
                try {
                    const response = await fetch('/api/server/logs');
                    const result = await response.json();
                    
                    if (result.logs && result.logs.length > 0) {
                        const logContainer = document.getElementById('serverLog');
                        const currentLogs = logContainer.children.length;
                        
                        // Only add new logs
                        if (result.logs.length > currentLogs) {
                            for (let i = currentLogs; i < result.logs.length; i++) {
                                const logEntry = document.createElement('div');
                                logEntry.textContent = result.logs[i];
                                logContainer.appendChild(logEntry);
                            }
                            logContainer.scrollTop = logContainer.scrollHeight;
                        }
                    }
                } catch (error) {
                    console.error('Failed to fetch logs:', error);
                }
            }
        }, 1000); // Poll every second
    }

    stopLogPolling() {
        if (this.logPollingInterval) {
            clearInterval(this.logPollingInterval);
            this.logPollingInterval = null;
        }
    }

    async downloadFile(filename) {
        try {
            console.log(`Attempting to download: ${filename}`);
            const response = await fetch(`/api/files/download/${filename}`);
            
            console.log(`Response status: ${response.status}`);
            console.log(`Response headers:`, response.headers);
            
            if (response.ok) {
                // Check if response is actually a file (not JSON error)
                const contentType = response.headers.get('content-type');
                if (contentType && contentType.includes('application/json')) {
                    // This is an error response, not a file
                    const result = await response.json();
                    console.error(`Download failed: ${result.error}`);
                    this.showToast(`Download failed: ${result.error}`, 'error');
                    return;
                }
                
                const blob = await response.blob();
                console.log(`Blob size: ${blob.size} bytes`);
                
                if (blob.size === 0) {
                    this.showToast(`Download failed: File is empty`, 'error');
                    return;
                }
                
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                a.style.display = 'none';
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                
                this.showToast(`Downloaded ${filename}`, 'success');
            } else {
                // Try to get error message from response
                try {
                    const result = await response.json();
                    console.error(`Download failed: ${result.error}`);
                    this.showToast(`Download failed: ${result.error}`, 'error');
                } catch (jsonError) {
                    console.error(`Download failed: HTTP ${response.status}`);
                    this.showToast(`Download failed: HTTP ${response.status}`, 'error');
                }
            }
        } catch (error) {
            console.error(`Download error: ${error.message}`);
            this.showToast(`Download failed: ${error.message}`, 'error');
        }
    }

    async deleteFile(filename) {
        if (!confirm(`Are you sure you want to delete ${filename}?`)) {
            return;
        }

        try {
            console.log(`Deleting file: ${filename}`);
            const response = await fetch(`/api/files/delete/${filename}`, {
                method: 'DELETE'
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const result = await response.json();
            console.log('Delete response:', result);
            
            if (result.success) {
                this.showToast(`Deleted ${filename}`, 'success');
                // Refresh the file list immediately
                await this.refreshFileList();
            } else {
                this.showToast(`Delete failed: ${result.error}`, 'error');
            }
        } catch (error) {
            console.error('Error deleting file:', error);
            this.showToast(`Delete failed: ${error.message}`, 'error');
        }
    }

    updateStats() {
        document.getElementById('totalTransfers').textContent = this.stats.totalTransfers;
        document.getElementById('totalBytes').textContent = this.formatFileSize(this.stats.totalBytes);
        
        const successRate = this.stats.totalTransfers > 0 
            ? ((this.stats.successfulTransfers / this.stats.totalTransfers) * 100).toFixed(1)
            : 100;
        document.getElementById('successRate').textContent = `${successRate}%`;
        
        const avgSpeed = this.stats.transferTimes.length > 0
            ? (this.stats.totalBytes / this.stats.transferTimes.reduce((a, b) => a + b, 0) / 1024).toFixed(1)
            : 0;
        document.getElementById('avgSpeed').textContent = avgSpeed;
        
        this.saveStats();
    }

    loadStats() {
        const saved = localStorage.getItem('udpTransferStats');
        if (saved) {
            this.stats = { ...this.stats, ...JSON.parse(saved) };
            this.updateStats();
        }
    }

    saveStats() {
        localStorage.setItem('udpTransferStats', JSON.stringify(this.stats));
    }

    showToast(message, type = 'info') {
        const toastContainer = document.getElementById('toastContainer');
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        
        const icon = this.getToastIcon(type);
        toast.innerHTML = `
            <i class="fas ${icon}"></i>
            <span>${message}</span>
        `;
        
        toastContainer.appendChild(toast);
        
        setTimeout(() => {
            toast.style.animation = 'slideOut 0.3s ease forwards';
            setTimeout(() => {
                toastContainer.removeChild(toast);
            }, 300);
        }, 3000);
    }

    getToastIcon(type) {
        switch (type) {
            case 'success': return 'fa-check-circle';
            case 'error': return 'fa-exclamation-circle';
            case 'warning': return 'fa-exclamation-triangle';
            case 'info': return 'fa-info-circle';
            default: return 'fa-info-circle';
        }
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
}

// Initialize the UI when the page loads
let ui;
document.addEventListener('DOMContentLoaded', () => {
    ui = new UDPTransferUI();
    
    // Add slideOut animation for toasts
    const style = document.createElement('style');
    style.textContent = `
        @keyframes slideOut {
            from {
                transform: translateX(0);
                opacity: 1;
            }
            to {
                transform: translateX(100%);
                opacity: 0;
            }
        }
    `;
    document.head.appendChild(style);
    
    // Add cleanup when window is closed
    window.addEventListener('beforeunload', async (event) => {
        // Stop the UDP server if it's running
        if (ui && ui.serverRunning) {
            try {
                await fetch('/api/server/stop', { method: 'POST' });
                console.log('UDP server stopped on window close');
            } catch (error) {
                console.error('Error stopping server on window close:', error);
            }
        }
        
        // Clear any intervals
        if (ui && ui.logPollingInterval) {
            clearInterval(ui.logPollingInterval);
        }
    });
});

// Global function for file download (called from HTML)
function downloadFile(filename) {
    if (ui) {
        ui.downloadFile(filename);
    }
}
