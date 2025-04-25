#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap(
    logger *logger) : _logger(logger)
{
    debug_with_guard("Allocator constructor called");
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    debug_with_guard("Attempting to allocate " + std::to_string(size) + " bytes");

    if (size == 0) {
        error_with_guard("Zero-size allocation requested");
        return nullptr;
    }

    void* ptr;

    try {
        ptr = ::operator new(size);
        debug_with_guard("Successfully allocated " + std::to_string(size) + " bytes at " +
                         std::to_string(reinterpret_cast<uintptr_t>(ptr)));
    } catch (const std::bad_alloc&) {
        error_with_guard("Failed to allocate " + std::to_string(size) + " bytes");
        throw;
    }

    return ptr;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    if (at == nullptr) {
        error_with_guard("Attempt to deallocate nullptr");
        return;
    }

    debug_with_guard("Deallocating memory at " + std::to_string(reinterpret_cast<uintptr_t>(at)));

    ::operator delete(at);

    debug_with_guard("Memory deallocated successfully");
}

inline logger *allocator_global_heap::get_logger() const
{
    return _logger;
}

inline std::string allocator_global_heap::get_typename() const
{
    return "allocator_global_heap";
}

allocator_global_heap::~allocator_global_heap()
{
    debug_with_guard("Allocator destructor called");
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other) :
        _logger(other._logger)
{
    debug_with_guard("Allocator copy constructor called");
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    debug_with_guard("Allocator copy assignment called");
    if (this != &other) {
        _logger = other._logger;
    }
    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    const allocator_global_heap* other_alloc = dynamic_cast<const allocator_global_heap*>(&other);
    return other_alloc != nullptr && this->_logger == other_alloc->_logger;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept :
        _logger(std::exchange(other._logger, nullptr))
{
    debug_with_guard("Allocator move constructor called");
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    debug_with_guard("Allocator move assignment called");
    if (this != &other) {
        _logger = std::exchange(other._logger, nullptr);
    }
    return *this;
}
