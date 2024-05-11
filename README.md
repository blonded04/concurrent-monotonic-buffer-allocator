# concurrent-monotonic-buffer-allocator
Simple thread-safe header-only C++-20 monotonic buffer allocator for testing STL-compliant concurrent data-structures.

Currently there are:
* `cmb_allocator`: concurrent monotonic buffer allocator;
* `cmb_multiallocator`: concurrent monotonic multibuffer allocator, may contain multiple buffers for better cache-coherence and less synchronization overhead.

## Usage

Just `#include <cmba/allocator.h>` to your code:

```c++
#include <cmba/allocator.h>

#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::size_t k_elements_count = 100000;

int main() {
    std::vector<std::byte> buffer(k_elements_count * 256);
    // also possible: cmba::cmb_resource
    cmba::concurrent_monotonic_buffer_resource resource(buffer.data(), buffer.size());

    // also possible: cmba::cmb_allocator<int>
    cmba::concurrent_monotonic_buffer_allocator<int> allocator_int(&resource);

    std::vector<int, cmba::cmb_allocator<int>> container_vector(allocator_int);
    container_vector.reserve(k_elements_count);
    for (int i = 0; i < k_elements_count; i++) {
        container_vector.push_back(i + 10);
    }
    container_vector.push_back(2004);  // reallocation happens
}
```

You may also want to see [other examples](https://github.com/blonded04/concurrent-monotonic-buffer-allocator/blob/master/example.cpp).

## License
This code is licensed under The Unlicense, which means that you are free to do anything you want with this code.
