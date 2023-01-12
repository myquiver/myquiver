FROM mysql:8.0.31-debian AS builder

RUN sed -i -e 's/^deb /deb-src /g' /etc/apt/sources.list.d/mysql.list \
    && apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt build-dep -y mysql-community \
    && apt-get -y install --no-install-recommends pkg-config curl wget sudo ca-certificates lsb-release git ninja-build

ENV ARROW_VERSION 10.0.1-1
RUN wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    && apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    && apt-get update && apt-get install -y -V \
        libarrow-dev=${ARROW_VERSION} libarrow-dataset-dev=${ARROW_VERSION} libparquet-dev=${ARROW_VERSION} \
    && rm -f ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb

WORKDIR /root

RUN mkdir -p /usr/global/share \
    && wget --no-verbose https://packages.groonga.org/tmp/boost/boost_1_77_0.tar.bz2 \
    && mv boost_1_77_0.tar.bz2 /usr/global/share/

RUN apt source mysql-community-server-core \
    && cd mysql-community-8.0.31 \
    && debian/rules override_dh_auto_configure \
    && make -C release/utilities -j$(nproc) GenError

COPY . /root/myquiver

RUN mkdir -p myquiver.build \
    && cmake -S myquiver -B myquiver.build \
        -DMY_QUIVER_MYSQL_SOURCE_DIR=/root/mysql-community-8.0.31 \
        -DMY_QUIVER_MYSQL_BUILD_DIR=/root/mysql-community-8.0.31/release \
        -DMY_QUIVER_MYSQLD=$(which mysqld) \
        -DCMAKE_INSTALL_PREFIX=/tmp/local \
        -DCMAKE_BUILD_TYPE=release \
        -DParquet_DIR=/usr/lib/x86_64-linux-gnu/cmake/arrow \
        -DArrowDataset_DIR=/usr/lib/x86_64-linux-gnu/cmake/arrow \
        -GNinja \
    && ninja -C myquiver.build

FROM mysql:8.0.31-debian

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends wget ca-certificates lsb-release

ENV ARROW_VERSION 10.0.1-1
RUN wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    && apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    && apt-get update && apt-get install -y -V libarrow-dataset1000 \
    && rm -f ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb

COPY --from=builder /root/myquiver.build/ha_my_quiver.so /usr/lib/mysql/plugin/
