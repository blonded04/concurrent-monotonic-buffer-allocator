#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <thread>
#include <utility>
#include <vector>
#include "cmba/allocator.hpp"

constexpr std::size_t k_elements_count = 1000000;

int main() {
    try {
        std::vector<std::byte> buffer(k_elements_count * 256);  // should be enough
        // also possible: cmba::cmb_resource
        cmba::concurrent_monotonic_buffer_resource resource(buffer.data(), buffer.size());

        // also possible: cmba::cmb_allocator
        cmba::concurrent_monotonic_buffer_allocator<int> allocator_int(&resource);
        cmba::cmb_allocator<std::pair<const int, int>> allocator_pair_int = allocator_int;

        std::vector<int, cmba::cmb_allocator<int>> container_vector(allocator_int);
        std::list<int, cmba::cmb_allocator<int>> container_list(allocator_int);

        std::map<int, int, std::less<>, cmba::cmb_allocator<std::pair<const int, int>>> container_map(
            allocator_pair_int);

        container_vector.reserve(k_elements_count);
        for (int i = 0; i < k_elements_count; i++) {
            container_vector.push_back(i + 10);
            container_list.emplace_back(i);
            container_map[i] = i + 20;
        }
        container_vector.push_back(2004);  // reallocation happens

        std::cout << "OK! {" << container_vector.size() << ", " << container_list.size() << ", "
                  << container_map.size() << "}\n";
    } catch (std::exception &e) {
        std::cout << "Something went wrong " << e.what() << "\n";
    }

    try {
        std::vector<std::byte> buffer(k_elements_count * 256);  // should be enough
        // also possible: cmba::cmb_resource
        cmba::cmb_resource resource(buffer.data(), buffer.size());

        // also possible: cmba::cmb_allocator
        cmba::cmb_allocator<int> allocator_int(&resource);

        constexpr std::size_t thread_count = 8;

        std::vector<std::thread> threads(thread_count);
        std::atomic<std::size_t> total_size;
        for (std::size_t i = 0; i < thread_count; i++) {
            threads[i] = std::thread([allocator_int, &total_size]() {
                std::vector<int, cmba::cmb_allocator<int>> container_vector(allocator_int);
                container_vector.reserve(k_elements_count / thread_count);

                for (int i = 0; i < k_elements_count / thread_count; i++) {
                    container_vector.push_back(i * 2 - 33);
                }

                total_size.fetch_add(container_vector.size(), std::memory_order_seq_cst);
            });
        }
        for (std::size_t i = 0; i < thread_count; i++) {
            threads[i].join();
        }

        std::cout << "OK! {" << total_size.load(std::memory_order_seq_cst) << "}\n";
    } catch (std::exception &e) {
    }
}
