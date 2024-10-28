clang++ -o eva-llvm eva-llvm.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core`

./eva-llvm

lli ./out.ll

echo $?

printf "\n"