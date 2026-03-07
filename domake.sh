#/bin/sh

#/bin/sh

# 1. Clean up old build artifacts
cd /home/louis/Documents/QtDesignStudio/examples/ClusterWorx/build

cmake -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 ..

cmake build -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 .

make -j$(nproc)

# 2. Set the environment (Order of paths is CRITICAL)
export QML_IMPORT_PATH=/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/imports
export LD_LIBRARY_PATH=/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/custom_libs:/home/louis/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH

# 3. Run directly from the build folder
./ClusterDashboardApp
