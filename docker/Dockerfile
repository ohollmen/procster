# docker build --rm=true -t 'procster:0.0.1' -f docker/Dockerfile .
# TEST_RUN:  docker run -it --rm --pid=host 'procster:0.0.1' sh
# FINAL_RUN: docker run --rm -p 8181:8181  --pid=host 'procster:0.0.1'
# ATTACH:    docker exec $CONTID sh
# - https://pkgs.alpinelinux.org/packages
# - https://github.com/gliderlabs/docker-alpine/issues/24
# - make: /bin/sh: Operation not permitted https://gitlab.alpinelinux.org/alpine/aports/-/issues/12396
# TODO: Multi-stage build, separate *-dev and RT libs
# NOTE: There seems to be problems with alpine:2.14 and make (make: /bin/sh: Operation not permitted)
# FROM alpine:3.14
FROM alpine:3.13 AS build

# Build tools (No make ?) install build-base ? alpine-sdk (superset of build-base) ?
RUN apk update && apk add --no-cache gcc make git
# TODO: Also avail: libmicrohttpd-static glib-static (NO jansson as static)
# *-dev
RUN apk add --no-cache procps procps-dev jansson jansson-dev glib glib-dev libmicrohttpd libmicrohttpd-dev libc-dev
#RUN apk add --no-cache procps jansson glib libmicrohttpd
WORKDIR /root/
RUN git clone https://github.com/ohollmen/procster.git
WORKDIR /root/procster
RUN make
# ENTRYPOINT ["/app/procster"]
CMD bash

# 2nd (install) stage
FROM alpine:3.13
# RT Stuff *only*
RUN apk update && apk add --no-cache procps jansson glib libmicrohttpd ca-certificates curl
RUN mkdir /root/procster
WORKDIR /root/procster
# Copy from build image
# *Must* use absolute paths, but *can* copy many.
# TODO (?): by manifest (.dockerignore with ! and ADD . ./) ... if built on host
COPY --from=0 /root/procster/procserver /root/procster/procs /root/procster/procster.conf.json /root/procster/README.md ./
CMD ./procserver 8181
