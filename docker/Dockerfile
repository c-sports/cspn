FROM debian:stretch
LABEL maintainer="CSPN Developers <dev@cspn.org>"
LABEL description="Dockerised CSPNCore, built from Travis"

RUN apt-get update && apt-get -y upgrade && apt-get clean && rm -fr /var/cache/apt/*

COPY bin/* /usr/bin/
