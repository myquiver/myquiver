FROM debian:bullseye

ARG USERNAME=codespace
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        man man-db manpages manpages-dev most \
        acl curl wget sudo fish zsh ca-certificates lsb-release \
        bison build-essential ccache cmake flex libncurses5-dev libssl-dev libz-dev libz-dev ninja-build pkg-config gdb \
        python3 python3-pip

ENV ARROW_VERSION 10.0.1-1
RUN wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    && apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    && apt-get update && apt-get install -y -V \
        libarrow-dev=${ARROW_VERSION} libarrow-dataset-dev=${ARROW_VERSION} libparquet-dev=${ARROW_VERSION} \
    && rm -f ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb

RUN python3 -m pip install pandas pyarrow parquet-tools

# https://github.com/cli/cli/blob/trunk/docs/install_linux.md
RUN curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null \
    && apt-get update && apt-get -y install gh

RUN groupadd --gid ${USER_GID} ${USERNAME} && \
    useradd -s /bin/bash --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} && \
    echo ${USERNAME} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USERNAME} && \
    chmod 0440 /etc/sudoers.d/${USERNAME}

ENV CMAKE_C_COMPILER_LAUNCHER ccache
ENV CMAKE_CXX_COMPILER_LAUNCHER ccache
ENV CCACHE_DIR /workspaces/.ccache
ENV PAGER most

USER ${USERNAME}
