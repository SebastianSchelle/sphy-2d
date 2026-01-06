cd "$(dirname "$0")"/.. &&
echo "Initializing bgfx repository..." &&
cd thirdparty/bgfx &&
git submodule init &&
git submodule update &&
mkdir -p build &&
cd build &&
echo "Configuring bgfx build..." &&
cmake .. &&
echo "Building bgfx..." &&
make -j16
