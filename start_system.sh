#!/bin/bash

#
# Parking-System startup script.
#
# Opens separate terminal windows and starts the required
# project modules for local development and testing.
#

# Absolute path to the project root directory.
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Start the Database module.
gnome-terminal \
    --title="Database" \
    -- bash -c "cd \"$PROJECT_DIR/Database\" && ./database_test; exec bash"

# Start the TCP Server module.
gnome-terminal \
    --title="TCP Server" \
    -- bash -c "cd \"$PROJECT_DIR/TcpServer\" && ./tcp_server_test; exec bash"

