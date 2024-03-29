// Valery Matskevich, 2024
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

#ifndef CMBA_BLONDED04_
#define CMBA_BLONDED04_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>

namespace cmba {
namespace detail {

constexpr inline std::size_t ceil_size_as(std::size_t size, std::size_t alignment) {
    const std::size_t remainder = size % alignment;

    if (remainder == 0) {
        return size;
    }
    return size + alignment - remainder;
}

inline void cpu_pause() {
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || \
    defined(_M_IX86)
    asm volatile(
        "pause\n"
        "pause\n"
        "pause\n"
        "pause");
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
    asm volatile(
        "yield\n"
        "yield\n"
        "yield\n"
        "yield\n");
#elif defined(__ppc__) || defined(_ARCH_PPC) || defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)
    asm volatile("or 27,27,27");
#elif defined(__sparc) || defined(__sparc__)
    asm volatile("membar #LoadLoad" : : : "memory");
#elif defined(__ia64__)
    asm volatile("hint @pause");
#else
    asm volatile("" : : : "memory");
#endif
}

constexpr std::size_t k_cacheline_size =
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

}  // namespace detail

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace detail;

struct monotonic_buffer_allocator_invalid_size_exception : public std::invalid_argument {
public:
    explicit monotonic_buffer_allocator_invalid_size_exception() noexcept
        : std::invalid_argument(
              "Concurrent monotonic buffer allocation failed, invalid allocation of size 0.") {
    }
};

struct monotonic_buffer_bad_alloc_exception : public std::bad_alloc {
private:
    std::size_t m_failed_alloc_size;
    std::size_t m_failed_alloc_alignment;

public:
    monotonic_buffer_bad_alloc_exception(std::size_t failed_alloc_size,
                                         std::size_t failed_alloc_alignment) noexcept
        : m_failed_alloc_size{failed_alloc_size}, m_failed_alloc_alignment{failed_alloc_alignment} {
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "Concurrent monotonic buffer allocation failed, out of memory. "
               "To get size and alignment of failed allocation call "
               "get_context() method";
    }

    [[nodiscard]] std::pair<std::size_t, std::size_t> get_context() const noexcept {
        return {m_failed_alloc_size, m_failed_alloc_alignment};
    }
};

class alignas(k_cacheline_size) concurrent_monotonic_buffer_resource {
private:
    std::byte* m_buffer;
    std::size_t m_size;
    alignas(k_cacheline_size) std::atomic<std::size_t> m_current_offset;

    std::byte* do_allocate(std::size_t bytes, std::size_t alignment) {
        std::size_t current_offset_snapshot = m_current_offset.load(std::memory_order_relaxed);
        std::size_t needed_offset =
            ceil_size_as(ceil_size_as(current_offset_snapshot, alignment) + bytes, k_cacheline_size);

        while (!m_current_offset.compare_exchange_strong(current_offset_snapshot, needed_offset,
                                                         std::memory_order_acquire)) {
            cpu_pause();

            current_offset_snapshot = m_current_offset.load(std::memory_order_relaxed);
            needed_offset =
                ceil_size_as(ceil_size_as(current_offset_snapshot, alignment) + bytes, k_cacheline_size);

            if (needed_offset > m_size) [[unlikely]] {
                throw monotonic_buffer_bad_alloc_exception(bytes, alignment);
            }
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return m_buffer + ceil_size_as(current_offset_snapshot, alignment);
    }

    constexpr void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) const noexcept {
    }

    bool do_is_equal(const concurrent_monotonic_buffer_resource& other) {
        return this == &other;
    }

    template <typename T>
    friend class concurrent_monotonic_buffer_allocator;

public:
    explicit concurrent_monotonic_buffer_resource(std::byte* buffer, std::size_t size) noexcept
        : m_buffer{buffer}, m_size{size}, m_current_offset{0} {
    }

    concurrent_monotonic_buffer_resource(const concurrent_monotonic_buffer_resource&) = delete;
    // already deleted implicitly because of atomic member (even though it might be possible to implement it)
    concurrent_monotonic_buffer_resource(concurrent_monotonic_buffer_resource&&) = delete;
    concurrent_monotonic_buffer_resource& operator=(const concurrent_monotonic_buffer_resource&) = delete;
    // already deleted implicitly because of atomic member (even though it might be possible to implement it)
    concurrent_monotonic_buffer_resource& operator=(concurrent_monotonic_buffer_resource&&) = delete;

    ~concurrent_monotonic_buffer_resource() = default;
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
            throw monotonic_buffer_allocator_invalid_size_exception();
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<T*>(m_resource->do_allocate(sizeof(T) * count, alignof(T)));
    }

    constexpr void deallocate(T* ptr, std::size_t count) const noexcept {
        m_resource->do_deallocate(ptr, sizeof(T) * count, alignof(T));
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

using cmb_resource = concurrent_monotonic_buffer_resource;

template <typename T>
using cmb_allocator = concurrent_monotonic_buffer_allocator<T>;

}  // namespace cmba

#endif  // CMBA_BLONDED04_
