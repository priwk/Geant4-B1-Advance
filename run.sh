rm -rf build
mkdir build
cd build
cp -r ../Input .
cmake ..
make
./B1
