#pragma once

#include <ios>
#include <stdint.h>
#include <stddef.h>

struct MyFakeIosBase : public std::ios_base
{
    MyFakeIosBase() : std::ios_base()
    {
    }
};

namespace CppBridge
{

    void initBridges();

    void *bridgeOperatorNew(size_t size);
    void *bridgeOperatorNewArray(size_t size);
    void bridgeOperatorDelete(void *ptr);
} // namespace CppBridge
