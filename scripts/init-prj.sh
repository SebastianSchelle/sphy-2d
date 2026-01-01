mkdir -p build
./scripts/init-cmdjson.sh
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
make -j16
