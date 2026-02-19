# mpolib2

Cross-platform library to perform common tasks like file I/O, processes/threads, networking, string conversion, etc. The goal is to abstract away the underlying platform.

## Caveat

This is a library I write for myself to use.  Therefore, backward compatibility isn't guaranteed.

## To build/install

These instructions assume you are installing to '/tmp/cmake_install/mpo'.  For Windows, you can substitute 'c:\temp\cmake_install' for '/tmp/cmake_install'.

```
mkdir build
cd build
cmake -DCMAKE_SYSTEM_PREFIX_PATH=/tmp/cmake_install -DCMAKE_INSTALL_PREFIX=/tmp/cmake_install/mpo ..
make install
```
