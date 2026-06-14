#pragma once

#include "ICubismAllocator.hpp"
#include <cstdlib>

namespace Csm = Live2D::Cubism::Framework;

class Live2DAllocator : public Csm::ICubismAllocator {
public:
    virtual void* Allocate(const Csm::csmSizeType size) override {
        return std::malloc(size);
    }

    virtual void Deallocate(void* memory) override {
        std::free(memory);
    }

    virtual void* AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment) override {
        size_t offset = alignment - 1 + sizeof(void*);
        void* original = std::malloc(size + offset);
        if (!original) return nullptr;

        void* aligned = reinterpret_cast<void*>(
            (reinterpret_cast<uintptr_t>(original) + sizeof(void*) + alignment - 1) & ~static_cast<uintptr_t>(alignment - 1));

        void** preamble = reinterpret_cast<void**>(aligned) - 1;
        *preamble = original;

        return aligned;
    }

    virtual void DeallocateAligned(void* alignedMemory) override {
        if (alignedMemory) {
            void** preamble = reinterpret_cast<void**>(alignedMemory) - 1;
            std::free(*preamble);
        }
    }
};