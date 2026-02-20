# Aether Programming Language - Docker Image
# 
# This Dockerfile creates a production-ready environment for building and
# running Aether programs. It includes the compiler, LSP server, and all
# necessary dependencies.
#
# Build: docker build -t aether:latest .
# Run:   docker run -it -v $(pwd):/work aether:latest
#
# For development, use Dockerfile.dev instead.

FROM gcc:13-bookworm

LABEL maintainer="Aether Development Team"
LABEL description="Aether Programming Language Compiler and Runtime"
LABEL version="0.5.0"

# Install build dependencies
RUN apt-get update && apt-get install -y \
    make \
    git \
    python3 \
    ccache \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /aether

# Copy source code
COPY . .

# Build compiler and tools using make
RUN make clean && \
    make -j$(nproc) compiler && \
    chmod +x build/aetherc && \
    echo "Aether compiler built successfully"

# Verify build
RUN ./build/aetherc --version

# Add compiler to PATH
ENV PATH="/aether/build:${PATH}"

# Create workspace directory
WORKDIR /work

# Default command: show help
CMD ["aetherc", "--help"]

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD aetherc --version || exit 1

# Metadata
VOLUME ["/work"]

# Example usage:
# docker run -it aether:latest bash
# docker run -v $(pwd):/work aether:latest aetherc /work/program.ae /work/output.c
# docker run -v $(pwd):/work aether:latest make -C /aether test

