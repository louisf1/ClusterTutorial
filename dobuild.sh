#/bin/sh

cd /home/louis/Documents/QtDesignStudio/examples/ClusterWorx/build
rm -rf *
cmake -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 ..
cmake build -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 .
make
LD_LIBRARY_PATH=. ./ClusterDashboardApp

