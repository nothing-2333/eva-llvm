#include "./src/EvaLLVM.hpp"

int main()
{
    std::string program = R"(
        (var x 42)

        (begin
            (var (x string) "Hello")
            (printf "Value: %s\n\n" x)
        )

        (printf "Value: %d\n\n" x)

        (set x 100)

        (printf "Value: %d\n\n" x)
    )";

    EvaLLVM vm;
    vm.exec(program);

    return 0;
}