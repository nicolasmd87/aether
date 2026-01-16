# Aether Docker Environment

Quick reference for using Aether with Docker.

## Quick Start

### Build the Image

```bash
# From project root
docker build -t aether:latest -f docker/Dockerfile .
```

### Run Interactive Shell

```bash
# Linux/Mac
docker run -it -v $(pwd):/aether aether:latest /bin/bash

# Windows PowerShell
docker run -it -v ${PWD}:/aether aether:latest /bin/bash

# Windows CMD
docker run -it -v %cd%:/aether aether:latest /bin/bash
```

### Build and Test

```bash
# Build compiler
docker run --rm -v $(pwd):/aether -w /aether aether:latest make compiler

# Run tests
docker run --rm -v $(pwd):/aether -w /aether aether:latest make test

# Clean and rebuild
docker run --rm -v $(pwd):/aether -w /aether aether:latest bash -c "make clean && make -j4 compiler"
```

## Docker Compose

### Start Development Environment

```bash
# Start all services
docker-compose -f docker/docker-compose.yml up -d

# Start specific service
docker-compose -f docker/docker-compose.yml up -d aether-dev

# Enter running container
docker-compose -f docker/docker-compose.yml exec aether-dev /bin/bash
```

### Run Commands

```bash
# Build
docker-compose -f docker/docker-compose.yml exec aether-dev make compiler

# Test
docker-compose -f docker/docker-compose.yml run aether-test

# Stop everything
docker-compose -f docker/docker-compose.yml down
```

## Available Images

### Production (Dockerfile)
- Based on GCC 13
- Includes: make, git, valgrind
- Size: ~300MB
- Use for: building, running compiler

### Development (Dockerfile.dev)
- Based on GCC 13
- Includes: make, gdb, valgrind, ccache, perf, python3
- Size: ~500MB
- Use for: development, debugging, profiling

### CI Testing (Dockerfile.ci)
- Based on Ubuntu 22.04
- Includes: gcc, clang, valgrind, sanitizers
- Size: ~400MB
- Use for: local CI testing, memory leak detection

## Local CI Testing

Run the same CI checks as GitHub Actions:

```bash
# Easy way - using the script
./scripts/run-ci-local.sh

# Or using Make
make docker-build-ci  # Build CI image
make docker-ci        # Run full CI suite

# Run specific checks
make ci              # Run CI without Docker (native)
make valgrind-check  # Run Valgrind (Linux only)
```

**Why use Docker for CI?**
- Valgrind only works on Linux
- macOS and Windows users can test with Valgrind via Docker
- Ensures identical environment to GitHub Actions CI

## Testing Docker Setup

```bash
# Run automated tests
./scripts/test/test_docker.sh    # Linux/Mac
.\scripts\test\test_docker.ps1   # Windows
```

## Troubleshooting

### Docker not found
```bash
# Install Docker Desktop
# https://www.docker.com/products/docker-desktop/
```

### Permission denied (Linux)
```bash
# Add user to docker group
sudo usermod -aG docker $USER
# Log out and back in
```

### Build fails
```bash
# Clean Docker cache
docker system prune -a
docker build --no-cache -t aether:latest -f docker/Dockerfile .
```

## More Information

- Full Docker guide: [docs/setup/DOCKER.md](../docs/setup/DOCKER.md)
- Windows setup: [docs/setup/WINDOWS_SETUP.md](../docs/setup/WINDOWS_SETUP.md)
- Main README: [README.md](../README.md)
