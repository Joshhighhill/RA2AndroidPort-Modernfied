# ra2lobbyd: static Go binary in an empty image (~7 MB). Build for the host's
# architecture, or cross-build with `docker buildx build --platform linux/arm64`.
FROM golang:1.22-alpine AS build
WORKDIR /src
COPY go.mod ./
COPY cmd ./cmd
COPY internal ./internal
ARG TARGETOS=linux
ARG TARGETARCH
RUN CGO_ENABLED=0 GOOS=$TARGETOS GOARCH=$TARGETARCH \
    go build -trimpath -ldflags="-s -w" -o /ra2lobbyd ./cmd/ra2lobbyd

FROM scratch
COPY --from=build /ra2lobbyd /ra2lobbyd
USER 65534:65534
EXPOSE 50000/tcp 50000/udp 50001/udp 50100-50999/udp
ENTRYPOINT ["/ra2lobbyd"]
