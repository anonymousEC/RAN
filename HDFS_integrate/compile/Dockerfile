FROM ubuntu:20.04

WORKDIR /root

ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_TERSE=true

######
# Install common dependencies from packages. Versions here are either
# sufficient or irrelevant.
#
# WARNING: DO NOT PUT JAVA APPS HERE! Otherwise they will install default
# Ubuntu Java. See Java section below!
######
RUN apt-get -q update && apt-get -q install -y \
    apt-utils \
    build-essential \
    bzip2 \
    curl \
    doxygen \
    fuse \
    g++ \
    gcc \
    git \
    gnupg-agent \
    libbz2-dev \
    libcurl4-openssl-dev \
    libfuse-dev \
    libprotobuf-dev \
    libprotoc-dev \
    libsnappy-dev \
    libssl-dev \
    libtool \
    locales \
    make \
    pinentry-curses \
    pkg-config \
    rsync \
    software-properties-common \
    sudo \
    zlib1g-dev \
    autoconf \
    automake \
    cmake \
    python3 \
    libisal2

#######
# OpenJDK 8
#######
RUN apt -q install -y openjdk-8-jdk maven
ENV JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
ENV JRE_HOME=${JAVA_HOME}/jre
ENV CLASSPATH=.:${JAVA_HOME}/lib:${JRE_HOME}/lib
ENV PATH=${JAVA_HOME}/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:${PATH}
ENV MAVEN_HOME=/usr/share/maven
ENV MAVEN_OPTS="-Xms256m -Xmx512m"
ENV PATH=${MAVEN_HOME}/bin:${PATH}

######
# Install Google Protobuf 2.5.0
######
RUN mkdir -p /opt/protobuf-src && \
    curl -L -s -S https://github.com/google/protobuf/releases/download/v2.5.0/protobuf-2.5.0.tar.gz -o /opt/protobuf.tar.gz && \
    tar xzf /opt/protobuf.tar.gz --strip-components 1 -C /opt/protobuf-src && \
    cd /opt/protobuf-src && ./configure --prefix=/opt/protobuf && make install

ENV PROTOBUF_HOME=/opt/protobuf
ENV PATH=${PATH}:/opt/protobuf/bin