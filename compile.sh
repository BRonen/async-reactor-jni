#!/usr/bin/env sh

javac -h . Reactor.java;
gcc -shared -fpic -o libreactor.so -Wall -Wextra -I"${JAVA_HOME}/include" -I"${JAVA_HOME}/include/linux" Reactor.c;
