// Valery Matskevich, 2024
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

#ifndef CMBA_BLONDED04_HPP_
#define CMBA_BLONDED04_HPP_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cmba {
namespace detail {

inline void cpu_pause() {
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || \
    defined(_M_IX86)
    asm volatile("pause" : : : "memory");
#elif defined(__ARM_ARCH_2__) || defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__) || \
    defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T) || defined(__ARM_ARCH_5_) ||   \
    defined(__ARM_ARCH_5E_) || defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_) ||  \
    defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) ||  \
    defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_7__) || \
    defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || \
    defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_7M__) || \
    defined(__ARM_ARCH_7S__) || defined(__aarch64__) || defined(_M_ARM64)
    asm volatile("isb sy" : : : "memory");
#elif defined(__ppc__) || defined(_ARCH_PPC) || defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)
    asm volatile("or 27,27,27" : : : "memory");
#elif defined(__sparc) || defined(__sparc__)
    asm volatile("membar #LoadLoad" : : : "memory");
#elif defined(__ia64__)
    asm volatile("hint @pause" : : : "memory");
#else
    asm volatile("" : : : "memory");
#endif
}

constexpr const std::size_t k_cacheline_size =
#ifdef __cpp_lib_hardware_interference_size
    std::hardware_destructive_interference_size;
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    32;
#elif defined(__x86_64__) || defined(_M_X64)
    64;
#elif defined(__ARM_ARCH_2__) || defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__) || \
    defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T) || defined(__ARM_ARCH_5_) ||   \
    defined(__ARM_ARCH_5E_) || defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_) ||  \
    defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) ||  \
    defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_7__) || \
    defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || \
    defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_7M__) || \
    defined(__ARM_ARCH_7S__) || defined(__aarch64__) || defined(_M_ARM64)
    128;
#else
    64;
#endif

constexpr std::size_t algin_size_as(std::size_t size, std::size_t alignment) {
    if (size % alignment == 0) {
        return size;
    }
    return size + (alignment - size % alignment);
}

}  // namespace detail

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace detail;

class alignas(k_cacheline_size) concurrent_monotonic_buffer_resource {
private:
    std::byte* m_buffer;
    const std::size_t m_size;
    alignas(k_cacheline_size) std::atomic<std::size_t> m_current_offset;

    std::byte* do_allocate(std::size_t size, std::size_t alignment, std::size_t count) {
        const std::size_t actual_size = std::lcm(size, alignment);
        std::size_t current_offset_snapshot = m_current_offset.load(std::memory_order_relaxed);
        std::size_t desired_offset = 0;

        bool first_iteration = true;
        do {
            desired_offset = algin_size_as(
                algin_size_as(current_offset_snapshot, k_cacheline_size) + count * actual_size, actual_size);

            if (desired_offset > m_size) [[unlikely]] {
                throw std::bad_alloc();
            }

            if (!first_iteration) {
                cpu_pause();
            } else {
                first_iteration = false;
            }
        } while (!m_current_offset.compare_exchange_weak(current_offset_snapshot, desired_offset,
                                                         std::memory_order_acq_rel));

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return m_buffer + algin_size_as(current_offset_snapshot, alignment);
    }

    constexpr void do_deallocate(void*, std::size_t, std::size_t, std::size_t) const noexcept {
    }

    bool do_is_equal(const concurrent_monotonic_buffer_resource& other) {
        return this == &other;
    }

    template <typename Alloc>
    friend class concurrent_monotonic_multibuffer_resource;

    template <typename T>
    friend class concurrent_monotonic_buffer_allocator;

public:
    explicit concurrent_monotonic_buffer_resource(std::byte* buffer, std::size_t size) noexcept
        : m_buffer{buffer}, m_size{size}, m_current_offset{0} {
    }

    concurrent_monotonic_buffer_resource(const concurrent_monotonic_buffer_resource&) = delete;
    concurrent_monotonic_buffer_resource(concurrent_monotonic_buffer_resource&&) = delete;
    concurrent_monotonic_buffer_resource& operator=(const concurrent_monotonic_buffer_resource&) = delete;
    concurrent_monotonic_buffer_resource& operator=(concurrent_monotonic_buffer_resource&&) = delete;

    ~concurrent_monotonic_buffer_resource() = default;
};

template <typename Alloc = std::allocator<concurrent_monotonic_buffer_resource*>>
class alignas(k_cacheline_size) concurrent_monotonic_multibuffer_resource {
private:
    std::vector<concurrent_monotonic_buffer_resource*, Alloc> m_resources;
    std::unordered_map<std::thread::id, std::size_t> m_parent_buffers;

    std::byte* do_allocate(std::size_t size, std::size_t alignment, std::size_t count) {
        std::size_t parent_buffer = m_parent_buffers[std::this_thread::get_id()];
        return m_resources[parent_buffer]->do_allocate(size, alignment, count);
    }

    constexpr void do_deallocate(void*, std::size_t, std::size_t, std::size_t) const noexcept {
    }

    bool do_is_equal(const concurrent_monotonic_multibuffer_resource& other) {
        return this == &other;
    }

    template <typename U, typename UAlloc>
    friend class concurrent_monotonic_multibuffer_allocator;

public:
    explicit concurrent_monotonic_multibuffer_resource(
        const std::vector<concurrent_monotonic_buffer_resource*, Alloc>& resources)
        : m_resources{resources} {
        if (m_resources.empty()) [[unlikely]] {
            throw std::invalid_argument(
                "Can't create concurrent_monotonic_multibuffer_resource with no owned resources");
        }
        for (auto& resource : m_resources) {
            if (resource == nullptr) {
                throw std::invalid_argument(
                    "concurrent_monotonic_multibuffer_resource can't contain nullptr resources");
            }
        }
    }

    explicit concurrent_monotonic_multibuffer_resource(
        std::vector<concurrent_monotonic_buffer_resource*, Alloc>&& resources)
        : m_resources{std::move(resources)} {
        if (m_resources.empty()) [[unlikely]] {
            throw std::invalid_argument(
                "Can't create concurrent_monotonic_multibuffer_resource with no owned resources");
        }
        for (const auto& resource : m_resources) {
            if (resource == nullptr) {
                throw std::invalid_argument(
                    "concurrent_monotonic_multibuffer_resource can't contain nullptr resources");
            }
        }
    }

    concurrent_monotonic_multibuffer_resource(const concurrent_monotonic_multibuffer_resource&) = delete;
    concurrent_monotonic_multibuffer_resource(concurrent_monotonic_multibuffer_resource&&) = delete;
    concurrent_monotonic_multibuffer_resource& operator=(const concurrent_monotonic_multibuffer_resource&) =
        delete;
    concurrent_monotonic_multibuffer_resource& operator=(concurrent_monotonic_multibuffer_resource&&) =
        delete;

    ~concurrent_monotonic_multibuffer_resource() = default;

    void set_parent_buffer(std::size_t parent_buffer) {
        if (parent_buffer >= m_resources.size()) [[unlikely]] {
            throw std::invalid_argument("Not enough buffers in concurrent_monotonic_multibuffer_resource");
        }
        m_parent_buffers[std::this_thread::get_id()] = parent_buffer;
    }
};

template <typename T>
class concurrent_monotonic_buffer_allocator {
private:
    concurrent_monotonic_buffer_resource* m_resource;

    template <typename U>
    friend class concurrent_monotonic_buffer_allocator;

public:
    using value_type = T;

    explicit concurrent_monotonic_buffer_allocator(concurrent_monotonic_buffer_resource* resource) noexcept
        : m_resource{resource} {
    }

    template <typename U>
    concurrent_monotonic_buffer_allocator(const concurrent_monotonic_buffer_allocator<U>& other) noexcept {
        m_resource = other.m_resource;
    }

    T* allocate(std::size_t count) {
        if (count == 0) [[unlikely]] {
            throw std::invalid_argument("Can't allocate 0 elements");
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<T*>(m_resource->do_allocate(sizeof(T), alignof(T), count));
    }

    constexpr void deallocate(T* ptr, std::size_t count) const noexcept {
        m_resource->do_deallocate(ptr, sizeof(T), alignof(T), count);
    }
};

template <typename T, typename U>
constexpr bool operator==(const concurrent_monotonic_buffer_allocator<T>& lhs,
                          const concurrent_monotonic_buffer_allocator<U>& rhs) {
    return lhs.m_resource == rhs.m_resource;
}

template <typename T, typename U>
constexpr bool operator!=(const concurrent_monotonic_buffer_allocator<T>& lhs,
                          const concurrent_monotonic_buffer_allocator<U>& rhs) {
    return lhs.m_resource != rhs.m_resource;
}

template <typename T, typename Alloc = std::allocator<concurrent_monotonic_buffer_resource*>>
class concurrent_monotonic_multibuffer_allocator {
private:
    concurrent_monotonic_multibuffer_resource<Alloc>* m_resource;

    template <typename U, typename UAlloc>
    friend class concurrent_monotonic_multibuffer_allocator;

public:
    using value_type = T;

    explicit concurrent_monotonic_multibuffer_allocator(
        concurrent_monotonic_multibuffer_resource<Alloc>* resource) noexcept
        : m_resource{resource} {
    }

    template <typename U>
    concurrent_monotonic_multibuffer_allocator(
        const concurrent_monotonic_multibuffer_allocator<U, Alloc>& other) noexcept {
        m_resource = other.m_resource;
    }

    T* allocate(std::size_t count) {
        if (count == 0) [[unlikely]] {
            throw std::invalid_argument("Can't allocate 0 elements");
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<T*>(m_resource->do_allocate(sizeof(T), alignof(T), count));
    }

    constexpr void deallocate(T* ptr, std::size_t count) const noexcept {
        m_resource->do_deallocate(ptr, sizeof(T), alignof(T), count);
    }
};

template <typename T, typename U, typename Alloc>
constexpr bool operator==(const concurrent_monotonic_multibuffer_allocator<T, Alloc>& lhs,
                          const concurrent_monotonic_multibuffer_allocator<U, Alloc>& rhs) {
    return lhs.m_resource == rhs.m_resource;
}

template <typename T, typename U, typename Alloc>
constexpr bool operator!=(const concurrent_monotonic_multibuffer_allocator<T, Alloc>& lhs,
                          const concurrent_monotonic_multibuffer_allocator<T, Alloc>& rhs) {
    return lhs.m_resource != rhs.m_resource;
}

using cmb_resource = concurrent_monotonic_buffer_resource;
template <typename Alloc>
using cmb_multiresource = concurrent_monotonic_multibuffer_resource<Alloc>;

template <typename T>
using cmb_allocator = concurrent_monotonic_buffer_allocator<T>;
template <typename T, typename Alloc>
using cmb_multiallocator = concurrent_monotonic_multibuffer_allocator<T, Alloc>;

}  // namespace cmba

#endif  // CMBA_BLONDED04_HPP_
