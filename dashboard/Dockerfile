FROM alpine

RUN apk add --update --no-cache php5 php5-json

COPY . /srf-ip-conn-srv/dashboard
COPY config-example.inc.php /srf-ip-conn-srv/dashboard/config.inc.php

ENTRYPOINT ["/usr/bin/php5", "-S", "0.0.0.0:80", "-t", "/srf-ip-conn-srv/dashboard"]
