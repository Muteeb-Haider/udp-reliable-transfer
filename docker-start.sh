#!/bin/bash

echo "Starting UDP Reliable Transfer System with Docker..."
echo "=================================================="

# Check if Docker is running
if ! docker version >/dev/null 2>&1; then
    echo "Error: Docker is not running or not installed"
    echo "Please start Docker and try again"
    exit 1
fi

# Create necessary directories
mkdir -p server_data sample_data

# Build and start the system
echo "Building Docker image..."
docker-compose build

echo "Starting services..."
docker-compose up -d

echo ""
echo "=================================================="
echo "System is starting up..."
echo ""
echo "Web UI will be available at: http://localhost:5000"
echo "UDP Server will be available on port: 9000"
echo ""
echo "To view logs: docker-compose logs -f"
echo "To stop: docker-compose down"
echo "=================================================="

# Wait a moment and open browser (if available)
sleep 5
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open http://localhost:5000
elif command -v open >/dev/null 2>&1; then
    open http://localhost:5000
fi

echo ""
echo "Press Enter to view logs or Ctrl+C to exit..."
read
docker-compose logs -f
