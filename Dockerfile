FROM alpine

RUN apk add --update --no-cache git build-base gcc abuild binutils cmake

RUN git clone https://github.com/zserge/jsmn && git clone https://github.com/sharkrf/srf-ip-conn

COPY . /srf-ip-conn-srv

RUN cd /srf-ip-conn-srv/build && JSMN_PATH="/jsmn" SRF_IP_CONN_PATH="/srf-ip-conn" ./build-release.sh

ENTRYPOINT ["/srf-ip-conn-srv/build/Release/srf-ip-conn-srv",  "-f",  "-c", "/srf-ip-conn-srv/config-example.json"]
