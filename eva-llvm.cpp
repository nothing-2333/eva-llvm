#include "./src/EvaLLVM.hpp"

int main()
{
    std::string program = R"(
        (print "Hello, world!")
    )";

    EvaLLVM vm;
    vm.exec(program);

    return 0;
}