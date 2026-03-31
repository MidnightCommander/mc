# Sample Dockerfile demonstrating syntax highlighting features
# Exercises all tree-sitter captures from dockerfile-highlights.scm

# FROM with image name, tag, digest, and alias
FROM ubuntu:22.04 AS base
FROM node:18-alpine AS builder
FROM python@sha256:abcdef1234567890 AS runner

# ARG before and after FROM
ARG VERSION=1.0
ARG BUILD_DATE

# MAINTAINER (deprecated)
MAINTAINER old-maintainer@example.com

# LABEL with key-value pairs
LABEL maintainer="user@example.com"
LABEL version="1.0"
LABEL description="A sample image"

# ENV variables
ENV APP_HOME=/app
ENV NODE_ENV=production
ENV PATH=$APP_HOME/bin:$PATH

# WORKDIR
WORKDIR /app
WORKDIR ${APP_HOME}

# USER
USER appuser
USER 1000:1000

# RUN with various forms
RUN apt-get update && apt-get install -y curl wget
RUN --mount=type=cache,target=/var/cache/apt apt-get install -y git
RUN --network=none echo "isolated build"
RUN ["sh", "-c", "echo hello"]

# COPY with options
COPY package.json .
COPY --from=builder /app/dist ./dist
COPY --chown=appuser:appgroup src/ ./src/
COPY --chmod=755 entrypoint.sh /usr/local/bin/
COPY --link config/ ./config/

# ADD with options
ADD https://example.com/file.tar.gz /tmp/
ADD --checksum=sha256:abcdef archive.tar.gz /app/

# EXPOSE ports
EXPOSE 8080
EXPOSE 443/tcp
EXPOSE 3000

# VOLUME
VOLUME /data
VOLUME ["/var/log", "/var/data"]

# STOPSIGNAL
STOPSIGNAL SIGTERM

# HEALTHCHECK
HEALTHCHECK --interval=30s --timeout=10s CMD curl -f http://localhost:8080/ || exit 1

# ONBUILD
ONBUILD RUN npm install

# SHELL
SHELL ["/bin/bash", "-c"]

# Variable expansion
RUN echo "Version: ${VERSION}"
RUN echo "Home: $APP_HOME"

# ENTRYPOINT and CMD
ENTRYPOINT ["python", "-m", "app"]
CMD ["--port", "8080"]
