#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${ENV_FILE:-$HOME/.amnod.env}"
DOCKERFILE="${DOCKERFILE:-$ROOT_DIR/Dockerfile}"
IMAGE_REPO="${IMAGE_REPO:-armoniax/amnod}"
BUILD_CONTEXT="${BUILD_CONTEXT:-$ROOT_DIR}"
BUILD_JOBS="${BUILD_JOBS:-2}"
NO_CACHE="${NO_CACHE:-false}"
PULL="${PULL:-false}"

usage() {
  cat <<'EOF'
Usage:
  ./build-image.sh [version]

Environment variables:
  ENV_FILE      Env file to source. Default: ~/.amnod.env
  IMAGE_REPO    Docker image repo. Default: armoniax/amnod
  DOCKERFILE    Dockerfile path. Default: ./Dockerfile
  BUILD_CONTEXT Docker build context. Default: repo root
  BUILD_JOBS    Passed into docker build as build arg. Default: 2
  NO_CACHE      true/false, enable docker --no-cache. Default: false
  PULL          true/false, enable docker --pull. Default: false

Examples:
  ./build-image.sh
  ./build-image.sh 1.0.5
  NO_CACHE=true BUILD_JOBS=1 ./build-image.sh
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  . "$ENV_FILE"
fi

VER="${1:-${VER:-}}"
if [[ -z "$VER" ]]; then
  echo "VER is empty. Pass it as the first argument or define it in $ENV_FILE." >&2
  exit 1
fi

if [[ ! -f "$DOCKERFILE" ]]; then
  echo "Dockerfile not found: $DOCKERFILE" >&2
  exit 1
fi

mkdir -p "$ROOT_DIR/logs"
LOG_FILE="$ROOT_DIR/logs/docker-build-${VER}-$(date +%Y%m%d-%H%M%S).log"
IMAGE_TAG="${IMAGE_REPO}:${VER}"

echo "Build config:"
echo "  version:      $VER"
echo "  image:        $IMAGE_TAG"
echo "  dockerfile:   $DOCKERFILE"
echo "  context:      $BUILD_CONTEXT"
echo "  build jobs:   $BUILD_JOBS"
echo "  no cache:     $NO_CACHE"
echo "  pull latest:  $PULL"
echo "  log file:     $LOG_FILE"

DOCKER_ARGS=(
  build
  --progress=plain
  --build-arg "VER=$VER"
  --build-arg "BUILD_JOBS=$BUILD_JOBS"
  -t "$IMAGE_TAG"
  -f "$DOCKERFILE"
)

if [[ "$NO_CACHE" == "true" ]]; then
  DOCKER_ARGS+=(--no-cache)
fi

if [[ "$PULL" == "true" ]]; then
  DOCKER_ARGS+=(--pull)
fi

DOCKER_ARGS+=("$BUILD_CONTEXT")

(
  cd "$ROOT_DIR"
  docker "${DOCKER_ARGS[@]}" 2>&1 | tee "$LOG_FILE"
)

echo "Build finished. Full log: $LOG_FILE"
