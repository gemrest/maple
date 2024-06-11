FROM alpine:latest as environment

RUN apk update \
  && apk upgrade \
  && apk add --no-cache libstdc++

FROM environment as build_environment

RUN apk add --no-cache \
  clang \
  ninja \
  alpine-sdk \
  openssl-dev \
  linux-headers

FROM build_environment as builder

WORKDIR /maple

COPY ./ ./

RUN ninja

FROM environment

WORKDIR /maple

COPY --from=builder /maple/build/maple ./

EXPOSE 1965

ENTRYPOINT ["./maple"]
