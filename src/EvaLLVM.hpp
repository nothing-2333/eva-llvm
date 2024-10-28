#pragma once

#include <string>
#include <memory>
#include <iostream>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

class EvaLLVM
{
public:
    EvaLLVM() 
    { 
        moduleInit(); 
        setupExternFunctions();
    }

    /**
     * 执行程序
     */
    void exec(const std::string& program)
    {

        // 编译成 LLVM IR
        compile();

        // 保存 IR 到文件
        module->print(llvm::outs(), nullptr);
        std::cout << "\n";

        saveModuleToFile("./out.ll");
    }
private:
    void compile()
    {
        auto fn = createFunction("main", llvm::FunctionType::get(builder->getInt32Ty(), false));

        auto result = gen();

        builder->CreateRet(builder->getInt32(0));
    }

    /**
     * 编译循环
     */
    llvm::Value* gen()
    {
        // return builder->getInt32(42);

        auto str = builder->CreateGlobalString("Hello, world\n");
        auto printfFn = module->getFunction("printf");
        std::vector<llvm::Value*> args { str };

        return builder->CreateCall(printfFn, args);
    }

    /**
     * 定义额外的函数（来自 libc++）
     */
    void setupExternFunctions()
    {
        auto bytePtrTy = builder->getInt8Ty()->getPointerTo();
        module->getOrInsertFunction("printf", llvm::FunctionType::get(
            builder->getInt32Ty(), 
            bytePtrTy,
            true
        ));
    }

    /**
     * 创造一个函数
     */
    llvm::Function* createFunction(const std::string& fnName, llvm::FunctionType* fnType)
    {
        auto fn = module->getFunction(fnName);

        if (fn == nullptr)
        {
            fn = createFunctionProto(fnName, fnType);
        }
        createFunctionBlock(fn);
        return fn;
    }

    /**
     * 创建一个原型函数
     */
    llvm::Function* createFunctionProto(const std::string& fnName, llvm::FunctionType* fnType)
    {
        fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
            fnName, *module);

        verifyFunction(*fn);

        return fn;
    }

    /**
     * 创建函数体
     */
    void createFunctionBlock(llvm::Function* fn)
    {
        auto entry = createBB("entry", fn);
        builder->SetInsertPoint(entry);
    }

    /**
     * 创造基本快
     */
    llvm::BasicBlock* createBB(std::string name, llvm::Function* fn = nullptr)
    {
        return llvm::BasicBlock::Create(*ctx, name, fn);
    }

    /**
     * 保存 IR 到文件
     */
    void saveModuleToFile(const std::string& fileName)
    {
        std::error_code errorCode;
        llvm::raw_fd_ostream outLL(fileName, errorCode);
        module->print(outLL, nullptr);
    }

    /**
     * 初始化
     */
    void moduleInit()
    {
        ctx = std::make_unique<llvm::LLVMContext>();
        module = std::make_unique<llvm::Module>("EvaLLVM", *ctx);
        builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    }

    /**
     * 当前编译的函数
     */
    llvm::Function* fn;

    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
};