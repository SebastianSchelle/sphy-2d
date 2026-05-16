mkdir build
ln -s build/compile_commands.json compile_commands.json
cd "$(dirname "$0")"/.. &&
echo "Initializing bgfx repository..." &&
cd thirdparty/bgfx &&
git submodule init &&
git submodule update &&
mkdir -p build &&
cd build &&
echo "Configuring bgfx build..." &&
if [ "$(uname -s)" = "Darwin" ]; then
  cmake .. -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
else
  cmake ..
fi &&
echo "Building bgfx..." &&
cmake --build . -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 8)"


# cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
# cmake --build build/release -j 12 -t run-server