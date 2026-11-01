# Clean Ubuntu 24.04 environment for reproducible builds and tests (no root needed at run time).
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive LC_ALL=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential pkg-config libseccomp-dev strace python3-matplotlib ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN useradd -m ogrenci
WORKDIR /work
COPY . /work
RUN chown -R ogrenci:ogrenci /work
USER ogrenci
# strace needs ptrace: run with --cap-add SYS_PTRACE (docker-compose.yml does this).
CMD ["bash", "scripts/test.sh"]
