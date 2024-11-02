#include "./src/EvaLLVM.hpp"

int main()
{
    std::string program = R"(
        (var z 32)
        (var x (* z 10))

        // (if (== x 42)
        //     (set x 100)
        //     (set x 200)
        // )   
        (printf "X: %d\n" x)
        (printf "X > 320?: %d\n" (> x 320))
    )";

    EvaLLVM vm;
    vm.exec(program);

    return 0;
}