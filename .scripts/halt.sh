#!/usr/bin/bash

# Variables
APP_NAME="front-sensornest"
APP_DIR="/var/www/html/$APP_NAME"
LOG_FILE="$APP_DIR/halt-deployment.log"

log() {
  local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
  echo "[$timestamp] $1" | tee -a "$LOG_FILE"
}

handle_error() {
  log "ERROR: $1"
  exit 1
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

mkdir -p "$(dirname "$LOG_FILE")"

log "Starting halt process for $APP_NAME"

export PATH="$HOME/.bun/bin:$PATH"

export NVM_DIR="$HOME/.config/nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" || log "Warning: NVM script not found, continuing anyway"

cd $APP_DIR || handle_error "Directory $APP_DIR not found"
log "Running halt script inside $APP_DIR"

if ! command_exists pm2; then
  handle_error "PM2 is not installed. Cannot halt application."
fi

if pm2 list | grep -q "$APP_NAME"; then
  log "Stopping $APP_NAME application..."
  pm2 stop $APP_NAME || handle_error "Failed to stop $APP_NAME"
  log "Successfully stopped $APP_NAME"
else
  log "Application $APP_NAME is not running in PM2"
fi

log "Saving PM2 configuration..."
pm2 save || log "Warning: Failed to save PM2 configuration"

log "Halt process completed successfully"