#pragma once

#include <string>
#include <regex>
#include <memory>
#include <iostream>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "./parser/EvaParser.h"
#include "./Environment.h"

using syntax::EvaParser;
using Env = std::shared_ptr<Environment>;

#define GEN_BINARY_OP(Op, varName)              \
    do                                          \
    {                                           \
        auto op1 = gen(exp.list[1], env);       \
        auto op2 = gen(exp.list[2], env);       \
        return builder->Op(op1, op2, varName);  \
    } while (false)

class EvaLLVM
{
public:
    EvaLLVM() : parser(std::make_unique<EvaParser>())
    { 
        moduleInit(); 
        setupExternFunctions();
        setupGlobalEnvironment();
    }

    /**
     * 执行程序
     */
    void exec(const std::string& program)
    {
        // 利用 syntax-cli 解析出 ast
        auto ast = parser->parse("(begin " + program + ")");

        // 编译成 LLVM IR
        compile(ast);

        // 保存 IR 到文件
        module->print(llvm::outs(), nullptr);
        std::cout << "\n";

        saveModuleToFile("./out.ll");
    }
private:
    /**
     * 编译
     */
    void compile(const Exp& ast)
    {
        auto fn = createFunction("main", llvm::FunctionType::get(builder->getInt32Ty(),
             false), GlobalEnv);

        createGlobalVar("VERSION", builder->getInt32(42));
        gen(ast, GlobalEnv);

        builder->CreateRet(builder->getInt32(0));
    }

    /**
     * 编译循环
     */
    llvm::Value* gen(const Exp& exp, Env env)
    {
        switch (exp.type)
        {
        case ExpType::NUMBER:
            return builder->getInt32(exp.number);
        
        case ExpType::STRING:
        {
            auto re = std::regex("\\\\n");
            auto str = std::regex_replace(exp.string, re, "\n");

            return builder->CreateGlobalString(str);
        }


        case ExpType::SYMBOL:
        {
            if (exp.string == "true" || exp.string == "false")
            {
                return builder->getInt1(exp.string == "true" ? true : false);
            }
            else
            {
                auto varName = exp.string;
                auto value = env->lookup(varName);

                if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(value))
                {
                    return builder->CreateLoad(localVar->getAllocatedType(),
                        localVar, varName.c_str());
                }
                else if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value))
                {
                    return builder->CreateLoad(globalVar->getInitializer()->getType(),
                        globalVar, varName.c_str());
                }
            }
        }

        case ExpType::LIST:
        {
            auto tag = exp.list[0];

            if (tag.type == ExpType::SYMBOL)
            {
                auto op = tag.string;

                if (op == "+")
                {
                    GEN_BINARY_OP(CreateAdd, "tmpadd");
                }
                else if (op == "-")
                {
                    GEN_BINARY_OP(CreateSub, "tmpsub");
                }
                else if (op == "*")
                {
                    GEN_BINARY_OP(CreateMul, "tmpmul");
                }
                else if (op == "/")
                {
                    GEN_BINARY_OP(CreateSDiv, "tmpdiv");
                }
                else if (op == ">")
                {
                    GEN_BINARY_OP(CreateICmpUGT, "tmpcmp");
                }
                else if (op == "<")
                {
                    GEN_BINARY_OP(CreateICmpULT, "tmpcmp");
                }
                else if (op == "==")
                {
                    GEN_BINARY_OP(CreateICmpEQ, "tmpcmp");
                }
                else if (op == "!=")
                {
                    GEN_BINARY_OP(CreateICmpNE, "tmpcmp");
                }
                else if (op == ">=")
                {
                    GEN_BINARY_OP(CreateICmpUGE, "tmpcmp");
                }
                else if (op == "<=")
                {
                    GEN_BINARY_OP(CreateICmpULE, "tmpcmp");
                }

                if (op == "var")
                {
                    auto varNameDecl = exp.list[1];
                    auto varName = extractVarName(varNameDecl);
                    
                    auto init = gen(exp.list[2], env);

                    auto varTy = extractVarType(varNameDecl);

                    auto varBinding = allocVar(varName, varTy, env);

                    return builder->CreateStore(init, varBinding);
                }
                else if (op == "printf")
                {
                    auto printfFn = module->getFunction("printf");

                    std::vector<llvm::Value*> args{};

                    for (auto i = 1; i < exp.list.size(); ++i)
                    {
                        args.push_back(gen(exp.list[i], env));
                    }

                    return builder->CreateCall(printfFn, args);
                }
                else if (op == "begin")
                {
                    auto blockEnv = std::make_shared<Environment>(
                        std::map<std::string, llvm::Value*>{}, env);

                    llvm::Value* blockRes;
                    for (auto i = 1; i < exp.list.size(); ++i)
                    {
                        blockRes = gen(exp.list[i], blockEnv);
                    }
                    return blockRes;
                }
                else if (op == "set")
                {
                    auto value = gen(exp.list[2], env);

                    auto varName = exp.list[1].string;

                    auto varBinding = env->lookup(varName);

                    return builder->CreateStore(value, varBinding);
                }
            }
        }

        }
        return builder->getInt32(0);
    }

    /**
     * 取出变量名
     */
    std::string extractVarName(const Exp& exp)
    {
        return exp.type == ExpType::LIST ? exp.list[0].string : exp.string;
    }

    /**
     * 取出变量类型
     */
    llvm::Type* extractVarType(const Exp& exp)
    {
        return exp.type == ExpType::LIST ? getTypeFromString(exp.list[1].string) : builder->getInt32Ty();
    }

    /**
     * 辅助 extractVarType
     */
    llvm::Type* getTypeFromString(const std::string& type_)
    {
        if (type_ == "number")
        {
            return builder->getInt32Ty();
        }

        if (type_ == "string")
        {
            return builder->getInt8Ty()->getPointerTo();
        }

        return builder->getInt32Ty();
    }

    /**
     * 在栈上为变量申请一块内存
     */
    llvm::Value* allocVar(const std::string& name, llvm::Type* type_, Env env)
    {
        varsBuilder->SetInsertPoint(&fn->getEntryBlock());

        auto varAlloc = varsBuilder->CreateAlloca(type_, 0, name.c_str());

        env->define(name, varAlloc);

        return varAlloc;
    }

    /**
     * 创造一个全局变量
     */
    llvm::GlobalVariable* createGlobalVar(const std::string& name, llvm::Constant* init)
    {
        module->getOrInsertGlobal(name, init->getType());
        auto variable = module->getNamedGlobal(name);
        variable->setAlignment(llvm::MaybeAlign(4));
        variable->setConstant(false);
        variable->setInitializer(init);
        return variable;
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
    llvm::Function* createFunction(const std::string& fnName, llvm::FunctionType* fnType, Env env)
    {
        auto fn = module->getFunction(fnName);

        if (fn == nullptr)
        {
            fn = createFunctionProto(fnName, fnType, env);
        }
        createFunctionBlock(fn);
        return fn;
    }

    /**
     * 创建一个原型函数
     */
    llvm::Function* createFunctionProto(const std::string& fnName, llvm::FunctionType* fnType, Env env)
    {
        fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
            fnName, *module);

        verifyFunction(*fn);

        env->define(fnName, fn);

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
        varsBuilder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    }

    /**
     * 开启全局作用域
     */
    void setupGlobalEnvironment()
    {
        std::map<std::string, llvm::Value*> globalObject
        {
            {"VERSION", builder->getInt32(42)},
        };

        std::map<std::string, llvm::Value*> globalRec{};

        for (auto& entry : globalObject)
        {
            globalRec[entry.first] = createGlobalVar(entry.first, (llvm::Constant*)entry.second);
        }

        GlobalEnv = std::make_shared<Environment>(globalRec, nullptr);
    }

    std::unique_ptr<EvaParser> parser;
    std::shared_ptr<Environment> GlobalEnv;
    /**
     * 当前编译的函数
     */
    llvm::Function* fn;

    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> varsBuilder;
    std::unique_ptr<llvm::IRBuilder<>> builder;
};