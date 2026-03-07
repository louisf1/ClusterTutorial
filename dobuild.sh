#/bin/sh

export QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 # Set for the current shell session

cd /home/louis/Documents/QtDesignStudio/examples/ClusterWorx/build
rm -rf *
cmake -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 ..
cmake build -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 .
make
LD_LIBRARY_PATH=. ./ClusterDashboardApp

