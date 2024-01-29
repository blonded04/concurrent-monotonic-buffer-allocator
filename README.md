# concurrent-monotonic-buffer-allocator
Simple header-only C++20 concurrent monotonic buffer allocator that models the allocator requirements from the [[allocator.requirements]](https://eel.is/c++draft/allocator.requirements) ISO C++ section.

Feel free to open an issue or a pull-request.

## Usage

Just `#include <cmba/allocator.h>` to your code:

```c++
#include <cmba/allocator.h>

int main() {
    std::vector<std::byte> buffer(k_elements_count * 256);
    // also possible: cmba::cmb_resource
    cmba::concurrent_monotonic_buffer_resource resource(buffer.data(), buffer.size());

    // also possible: cmba::cmb_allocator
    cmba::concurrent_monotonic_buffer_allocator<int> allocator_int(&resource);

    container_vector.reserve(k_elements_count);
    for (int i = 0; i < k_elements_count; i++) {
        container_vector.push_back(i + 10);
    }
    container_vector.push_back(2004);  // reallocation happens
}
```

You may also want to see [other examples](https://github.com/blonded04/concurrent-monotonic-buffer-allocator/blob/master/example.cpp).

## License
This code is licensed under The Unlicense, which means that you are free to do anything you want with this code without facing any legal consequences.
