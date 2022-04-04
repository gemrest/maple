FROM alpine:latest as dependencies

RUN apk update \
    && apk upgrade \
    && apk add --no-cache \
    clang \
    ninja \
    alpine-sdk \
    openssl-dev \
    libstdc++

FROM dependencies as builder

WORKDIR /maple

COPY ./ ./

RUN ninja

FROM dependencies

WORKDIR /maple

COPY --from=builder /maple/out/maple ./

EXPOSE 1965

ENTRYPOINT ["./maple"]
