#!/usr/bin/env bash

docker run -it -v "$(pwd):/usr/src/bittorent" opensuse/tumbleweed sh -c "\
zypper in rpm-build cmake ninja gcc-c++ libcurl-devel \
&& cd /usr/src/bittorent \
&& rpmbuild -bb bittorent-by-dimka.spec \
"
