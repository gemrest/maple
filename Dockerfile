FROM alpine:3.23 AS builder

RUN apk add --no-cache clang g++ ninja pkgconf openssl-dev openssl python3

WORKDIR /source

COPY build.ninja ./
COPY maple ./maple
COPY tests ./tests

RUN ninja check

FROM alpine:3.23

RUN apk add --no-cache libstdc++ libssl3 libcrypto3 \
    && addgroup -S -g 1000 maple \
    && adduser -S -u 1000 -G maple maple

WORKDIR /maple

COPY --from=builder /source/build/maple /usr/local/bin/maple

USER maple

EXPOSE 1965

ENTRYPOINT ["maple"]
