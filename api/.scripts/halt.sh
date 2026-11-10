#!/usr/bin/bash

# Variables
APP_NAME="api-sensornest"
APP_DIR="/var/www/html/$APP_NAME"
LOG_FILE="$APP_DIR/halt-deployment.log"

# Log function to track execution
log() {
  local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
  echo "[$timestamp] $1" | tee -a "$LOG_FILE"
}

# Error handling function
handle_error() {
  log "ERROR: $1"
  exit 1
}

# Check if command exists
command_exists() {
  command -v "$1" >/dev/null 2>&1
}

# Create log directory if it doesn't exist
mkdir -p "$(dirname "$LOG_FILE")"

log "Starting halt process for $APP_NAME"

# Export the path to bun
export PATH="$HOME/.bun/bin:$PATH"

# Source NVM and bun environments
export NVM_DIR="$HOME/.config/nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" || log "Warning: NVM script not found, continuing anyway"

# Change to application directory
cd $APP_DIR || handle_error "Directory $APP_DIR not found"
log "Running halt script inside $APP_DIR"

# Check for PM2
if ! command_exists pm2; then
  handle_error "PM2 is not installed. Cannot halt application."
fi

# Check if the application is running in PM2
if pm2 list | grep -q "$APP_NAME"; then
  log "Stopping $APP_NAME application..."
  pm2 stop $APP_NAME || handle_error "Failed to stop $APP_NAME"
  log "Successfully stopped $APP_NAME"
else
  log "Application $APP_NAME is not running in PM2"
fi

# Save PM2 configuration
log "Saving PM2 configuration..."
pm2 save || log "Warning: Failed to save PM2 configuration"

log "Halt process completed successfully"