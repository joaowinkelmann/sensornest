#!/usr/bin/bash

APP_NAME="front-sensornest"
APP_DIR="/var/www/html/$APP_NAME"
LOG_FILE="$APP_DIR/run-deployment.log"
DIST_DIR="$APP_DIR/dist"

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

log "Starting deployment process for $APP_NAME"

export PATH="$HOME/.bun/bin:$PATH"
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" || handle_error "NVM script not found. Please install NVM first"

if ! command_exists nvm; then
  handle_error "NVM is not installed. Please install it first: https://github.com/nvm-sh/nvm#installing-and-updating"
fi

if ! command_exists bun; then
  log "Bun not found. Installing Bun..."
  curl -fsSL https://bun.sh/install | bash || handle_error "Failed to install Bun"
  export PATH="$HOME/.bun/bin:$PATH"
  log "Bun installed successfully"
fi

if ! command_exists pm2; then
  log "PM2 not found. Installing PM2..."
  npm install -g pm2 || handle_error "Failed to install PM2"
  log "PM2 installed successfully"
fi

cd $APP_DIR || handle_error "Directory $APP_DIR not found"
log "Running script inside $APP_DIR"

log "Setting up Node environment..."
nvm install --lts || handle_error "Failed to install LTS Node version"
nvm use --lts || handle_error "Failed to use LTS Node version"
log "Using Node $(node -v)"

log "Updating Bun and dependencies..."
bun upgrade || handle_error "Failed to upgrade Bun"
log "Running Bun $(bun --revision)"
bun install || handle_error "Failed to install dependencies"

log "Building application for production..."
export NODE_ENV=production
bun run build || handle_error "Build failed"
log "Build completed successfully"

if pm2 show $APP_NAME &> /dev/null; then
  log "Restarting $APP_NAME static server..."
  pm2 restart $APP_NAME || handle_error "Failed to restart application with PM2"
else
  log "Starting $APP_NAME static server..."
  pm2 serve $DIST_DIR 7772 --name "$APP_NAME" --spa || handle_error "Failed to start static server with PM2"
fi

log "Saving PM2 configuration..."
pm2 save || handle_error "Failed to save PM2 configuration"

log "Deployment completed successfully"