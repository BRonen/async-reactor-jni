#!/usr/bin/env sh

javac -h . Reactor.java;
gcc -shared -fpic \
    -luring \
    -o libreactor.so \
    -Wall -Wextra \
    -I"${JAVA_HOME}/include" \
    -I"${JAVA_HOME}/include/linux" \
    -I"/nix/store/mcsrzb3wsvwgz2d4k5qxnc3d6448bicx-liburing-2.12-dev/include/" \
    Reactor.c Reactor.h;
