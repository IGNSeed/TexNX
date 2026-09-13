#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <vector>

typedef struct Sha256Context {
    BCRYPT_ALG_HANDLE provider{nullptr};
    BCRYPT_HASH_HANDLE hash{nullptr};
    std::vector<unsigned char> object;
} Sha256Context;

inline void requireBcryptSuccess(const NTSTATUS status) {
    if (status < 0) {
        std::abort();
    }
}

inline void sha256ContextCreate(Sha256Context* output) {
    *output = {};
    requireBcryptSuccess(BCryptOpenAlgorithmProvider(
        &output->provider, BCRYPT_SHA256_ALGORITHM, nullptr, 0));

    ULONG objectSize = 0;
    ULONG copied = 0;
    requireBcryptSuccess(BCryptGetProperty(
        output->provider, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &copied,
        0));
    output->object.resize(objectSize);
    requireBcryptSuccess(BCryptCreateHash(
        output->provider, &output->hash, output->object.data(), objectSize,
        nullptr, 0, 0));
}

inline void sha256ContextUpdate(Sha256Context* context, const void* source,
                                std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(source);
    while (size != 0) {
        const auto chunk = static_cast<ULONG>(std::min<std::size_t>(
            size, std::numeric_limits<ULONG>::max()));
        requireBcryptSuccess(BCryptHashData(
            context->hash, const_cast<PUCHAR>(bytes), chunk, 0));
        bytes += chunk;
        size -= chunk;
    }
}

inline void sha256ContextGetHash(Sha256Context* context, void* destination) {
    requireBcryptSuccess(BCryptFinishHash(
        context->hash, static_cast<PUCHAR>(destination), 32, 0));
    BCryptDestroyHash(context->hash);
    BCryptCloseAlgorithmProvider(context->provider, 0);
    context->hash = nullptr;
    context->provider = nullptr;
    context->object.clear();
}
