syntax-cli -g src/parser/EvaGrammar.bnf -m LALR1 -o src/parser/EvaParser.h

clang++ -o eva-llvm eva-llvm.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -fexceptions

./eva-llvm

lli ./out.ll

echo $?

printf "\n"