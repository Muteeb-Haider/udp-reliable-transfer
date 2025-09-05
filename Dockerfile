# Multi-stage build for UDP Reliable Transfer with Web UI
FROM debian:bookworm-slim AS build

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Build C++ components
WORKDIR /src
COPY CMakeLists.txt .
COPY src ./src
RUN cmake -B build -S . && cmake --build build --config Release

# Runtime stage with Python and Web UI
FROM debian:bookworm-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-pip \
    python3-venv \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy built C++ executables
COPY --from=build /src/build/bin/client /app/client
COPY --from=build /src/build/bin/server /app/server

# Copy Python web UI
COPY ui/ ./ui/
COPY sample_data/ ./sample_data/
COPY server_data/ ./server_data/

# Create necessary directories
RUN mkdir -p /app/server_data /app/sample_data

# Install Python dependencies
RUN pip3 install --no-cache-dir --break-system-packages -r ui/requirements.txt

# Create startup script
RUN echo '#!/bin/bash\n\
echo "Starting UDP Reliable Transfer System..."\n\
echo "======================================"\n\
\n\
# Start the UDP server in background\n\
echo "Starting UDP server on port 9000..."\n\
/app/server --port 9000 --out /app/server_data &\n\
SERVER_PID=$!\n\
\n\
# Wait for server to start\n\
sleep 3\n\
\n\
# Start the web UI\n\
echo "Starting web UI on port 5000..."\n\
cd /app/ui\n\
python3 server.py &\n\
WEB_PID=$!\n\
\n\
# Wait for web UI to start\n\
sleep 3\n\
\n\
echo "System is ready!"\n\
echo "UDP Server: localhost:9000"\n\
echo "Web UI: http://localhost:5000"\n\
echo "======================================"\n\
\n\
# Keep container running and handle shutdown\n\
trap "echo \"Shutting down...\"; kill $SERVER_PID $WEB_PID; exit 0" SIGTERM SIGINT\n\
\n\
# Wait for processes\n\
wait $SERVER_PID $WEB_PID' > /app/start.sh && chmod +x /app/start.sh

# Expose ports
EXPOSE 5000 9000

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:5000/api/health || exit 1

# Default command
CMD ["/app/start.sh"]