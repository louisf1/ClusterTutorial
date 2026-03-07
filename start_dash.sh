#/bin/sh

cd /home/louis/Documents/QtDesignStudio/examples/ClusterWorx/build
##export QT_QPA_PLATFORM=wayland-egl
# 1. Link the system libraries

# 2. Set the environment (Order of paths is CRITICAL)
export QML_IMPORT_PATH=/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/imports
export LD_LIBRARY_PATH=/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/custom_libs:/home/louis/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH

# 3. Run directly from the build folder
./ClusterDashboardApp

