#/bin/sh

# 1. Clean up old build artifacts
cd /home/louis/Documents/QtDesignStudio/examples/ClusterWorx/build
rm -rf *
cmake -DCMAKE_PREFIX_PATH=~/Qt/6.10.2/gcc_64 ..
make -j$(nproc)

## 2. Set the environment (REVERSED PRIORITY)
# 2. Set the environment (Order is VITAL)
# We must point to the Qt 6.10.2 system QML folder for Qt5Compat and Studio Effects
export QT_DIR=/home/louis/Qt/6.10.2/gcc_64
export QML2_IMPORT_PATH=$QT_DIR/qml
export QML_IMPORT_PATH=/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/imports:$QML2_IMPORT_PATH
export LD_LIBRARY_PATH=$QT_DIR/lib:/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/custom_libs:$LD_LIBRARY_PATH
cp ../VSS_paths.txt .
# 3. Run directly from the build folder
./ClusterDashboardApp
