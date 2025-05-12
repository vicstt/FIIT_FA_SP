#include <not_implemented.h>
#include "../include/allocator_boundary_tags.h"

allocator_boundary_tags::~allocator_boundary_tags()
{
    if (!_trusted_memory) return;

    auto mutex_ptr = reinterpret_cast<std::mutex*>(
            reinterpret_cast<uint8_t*>(_trusted_memory) +
            sizeof(logger*) + sizeof(memory_resource*) +
            sizeof(fit_mode) + sizeof(size_t));
    mutex_ptr->~mutex();

    auto *parent_allocator = *reinterpret_cast<std::pmr::memory_resource**>(
            reinterpret_cast<uint8_t*>(_trusted_memory) + sizeof(logger*));
    size_t total_size = get_total_size();
    parent_allocator->deallocate(_trusted_memory, total_size);
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept :
        _trusted_memory(std::exchange(other._trusted_memory, nullptr)) {}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    if (this != &other)
    {
        this->~allocator_boundary_tags();
        _trusted_memory = std::exchange(other._trusted_memory, nullptr);
    }
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        logger *logger,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size == 0) {
        throw std::invalid_argument("Space size cannot be zero");
    }

    parent_allocator = parent_allocator ? parent_allocator : std::pmr::get_default_resource();

    size_t total_size = allocator_metadata_size + space_size;

    _trusted_memory = parent_allocator->allocate(total_size);

    auto *ptr = reinterpret_cast<std::byte *>(_trusted_memory);

    *reinterpret_cast<class logger **>(ptr) = logger;
    ptr += sizeof(class logger *);

    *reinterpret_cast<std::pmr::memory_resource **>(ptr) = parent_allocator;
    ptr += sizeof(memory_resource * );

    *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(ptr) = allocate_fit_mode;
    ptr += sizeof(allocator_with_fit_mode::fit_mode);

    *reinterpret_cast<size_t *>(ptr) = space_size;
    ptr += sizeof(size_t);

    new(reinterpret_cast<std::mutex *>(ptr)) std::mutex();
    ptr += sizeof(std::mutex);

    *reinterpret_cast<void **>(ptr) = nullptr;
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    std::lock_guard lock(get_mutex());

    if (get_logger()) {
        get_logger()->debug("Allocating " + std::to_string(size) + " bytes");
    }

    const size_t total_size = size + occupied_block_metadata_size;
    size_t allocator_size = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(_trusted_memory) +
                                                       sizeof(class logger*) + sizeof(memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode));

    if (total_size > allocator_size) {
        if (get_logger()) {
            get_logger()->error("Not enough memory (requested: " +
                                std::to_string(size) + ", available: " +
                                std::to_string(allocator_size - occupied_block_metadata_size) + ")");
        }
        throw std::bad_alloc();
    }

    char* suitable_block = reinterpret_cast<char*>(find_suitable_block(size, get_fit_mode()));

    if (!suitable_block) {
        if (get_logger()) {
            get_logger()->error("No suitable block found for size " +
                                std::to_string(size));
        }
        throw std::bad_alloc();
    }

    char* heap_start = reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    void* prev_block = nullptr;
    void* next_block = nullptr;

    if (suitable_block == heap_start) {
        next_block = *first_block_ptr;
    }
    else {
        void* current = *first_block_ptr;
        while (current != nullptr) {
            size_t current_size = *reinterpret_cast<size_t*>(current);
            char* current_end = reinterpret_cast<char*>(current) + occupied_block_metadata_size + current_size;

            if (current_end == suitable_block) {
                prev_block = current;
                next_block = *reinterpret_cast<void**>(reinterpret_cast<char*>(current) + sizeof(size_t));
                break;
            }

            current = *reinterpret_cast<void**>(reinterpret_cast<char*>(current) + sizeof(size_t));
        }
    }

    size_t available_size = 0;
    if (next_block) {
        available_size = reinterpret_cast<char*>(next_block) - suitable_block;
    } else {
        available_size = heap_start + allocator_size - suitable_block;
    }

    if (available_size - total_size < occupied_block_metadata_size) {
        size = available_size - occupied_block_metadata_size;
    }

    *reinterpret_cast<size_t*>(suitable_block) = size;
    *reinterpret_cast<void**>(suitable_block + sizeof(size_t)) = next_block;
    *reinterpret_cast<void**>(suitable_block + sizeof(size_t) + sizeof(void*)) = prev_block;
    *reinterpret_cast<void**>(suitable_block + sizeof(size_t) + 2*sizeof(void*)) = _trusted_memory;

    if (prev_block) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(prev_block) + sizeof(size_t)) = suitable_block;
    } else {
        *first_block_ptr = suitable_block;
    }

    if (next_block) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(next_block) + sizeof(size_t) + sizeof(void*)) = suitable_block;
    }

    return suitable_block;
}

void allocator_boundary_tags::do_deallocate_sm(
    void *at)
{
    if (!at || !_trusted_memory) return;

    std::lock_guard<std::mutex> lock(get_mutex());

    if (get_logger()) {
        get_logger()->debug("Deallocating bytes");
    }

    char* heap_base = reinterpret_cast<char*>(_trusted_memory);
    size_t heap_size = *reinterpret_cast<size_t*>(
            heap_base + sizeof(logger*) + sizeof(memory_resource*) +
            sizeof(allocator_with_fit_mode::fit_mode));
    char* heap_start = heap_base + allocator_metadata_size;
    char* heap_end = heap_start + heap_size;

    if (reinterpret_cast<char*>(at) < heap_start || reinterpret_cast<char*>(at) >= heap_end) {
        throw std::logic_error("Invalid block address");
    }

    char* block = reinterpret_cast<char*>(at);
    void* next = *reinterpret_cast<void**>(block + sizeof(size_t));
    void* prev = *reinterpret_cast<void**>(block + sizeof(size_t) + sizeof(void*));

    if (prev) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(prev) + sizeof(size_t)) = next;
    } else {
        *reinterpret_cast<void**>(heap_start - sizeof(void*)) = next;
    }

    if (next) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(next) + sizeof(size_t) + sizeof(void*)) = prev;
    }
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
            reinterpret_cast<char*>(_trusted_memory) +
            sizeof(logger*) + sizeof(memory_resource*)
    ) = mode;

}


std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> blocks;
    if (!_trusted_memory) return blocks;

    char* const heap_start = reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
    const size_t heap_size = *reinterpret_cast<size_t*>(
            reinterpret_cast<char*>(_trusted_memory) +
            sizeof(logger*) + sizeof(memory_resource*) + sizeof(fit_mode));
    char* const heap_end = heap_start + heap_size;

    for (void* curr = *reinterpret_cast<void**>(heap_start - sizeof(void*));
         curr && curr < heap_end;
         curr = *reinterpret_cast<void**>(reinterpret_cast<char*>(curr) + sizeof(size_t)))
    {
        blocks.push_back({
                                 .block_size = static_cast<size_t>(
                                         *reinterpret_cast<size_t*>(curr) + occupied_block_metadata_size),
                                 .is_block_occupied = true
                         });
    }

    char* prev_end = heap_start;
    for (void* curr = *reinterpret_cast<void**>(heap_start - sizeof(void*));
         curr && curr < heap_end;
         prev_end = reinterpret_cast<char*>(curr) + occupied_block_metadata_size +
                    *reinterpret_cast<size_t*>(curr),
                 curr = *reinterpret_cast<void**>(reinterpret_cast<char*>(curr) + sizeof(size_t)))
    {
        if (reinterpret_cast<char*>(curr) > prev_end) {
            blocks.push_back({
                                     .block_size = static_cast<size_t>(
                                             reinterpret_cast<char*>(curr) - prev_end),
                                     .is_block_occupied = false
                             });
        }
    }

    if (prev_end < heap_end) {
        blocks.push_back({
                                 .block_size = static_cast<size_t>(heap_end - prev_end),
                                 .is_block_occupied = false
                         });
    }

    return blocks;
}

inline logger *allocator_boundary_tags::get_logger() const
{
    return *reinterpret_cast<class logger**>(_trusted_memory);
}

inline std::string allocator_boundary_tags::get_typename() const noexcept
{
    return "allocator_boundary_tags";
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    return boundary_iterator(reinterpret_cast<uint8_t*>(_trusted_memory) + allocator_metadata_size);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    size_t total_size = get_total_size();
    return boundary_iterator(reinterpret_cast<uint8_t*>(_trusted_memory) + total_size);
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other)
{
    std::lock_guard lock(other.get_mutex());

    auto *parent_allocator = *reinterpret_cast<std::pmr::memory_resource**>(
            reinterpret_cast<uint8_t*>(other._trusted_memory) + sizeof(logger*));

    size_t total_size = other.get_total_size();
    _trusted_memory = parent_allocator->allocate(total_size);
    std::memcpy(_trusted_memory, other._trusted_memory, total_size);

    new (reinterpret_cast<uint8_t*>(_trusted_memory) +
         sizeof(logger*) +
         sizeof(std::pmr::memory_resource*) +
         sizeof(fit_mode) +
         sizeof(size_t)) std::mutex;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other)
{
    if (this != &other)
    {
        std::lock_guard lock(get_mutex());
        this->~allocator_boundary_tags();
        new (this) allocator_boundary_tags(other);
    }
    return *this;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

bool allocator_boundary_tags::boundary_iterator::operator==(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr &&
           _trusted_memory == other._trusted_memory;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
        const allocator_boundary_tags::boundary_iterator & other) const noexcept
{
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (!_occupied_ptr || !_trusted_memory) return *this;

    size_t block_size = *reinterpret_cast<size_t*>(_occupied_ptr) & ~size_t(1);
    uint8_t* new_ptr = reinterpret_cast<uint8_t*>(_occupied_ptr) + block_size;

    size_t total_size = *reinterpret_cast<size_t*>(
            reinterpret_cast<uint8_t*>(_trusted_memory) +
            sizeof(logger*) + sizeof(memory_resource*) + sizeof(fit_mode));
    uint8_t* memory_end = reinterpret_cast<uint8_t*>(_trusted_memory) + total_size;

    if (new_ptr >= memory_end || new_ptr + occupied_block_metadata_size > memory_end) {
        _occupied_ptr = nullptr;
    } else {
        _occupied_ptr = new_ptr;
        _occupied = (*reinterpret_cast<size_t*>(_occupied_ptr) & size_t(1));
    }

    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (!_occupied_ptr || !_trusted_memory) return *this;

    uint8_t* memory_start = reinterpret_cast<uint8_t*>(_trusted_memory) +
                            sizeof(logger*) + sizeof(memory_resource*) + sizeof(fit_mode) +
                            sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);

    if (reinterpret_cast<uint8_t*>(_occupied_ptr) <= memory_start) {
        _occupied_ptr = nullptr;
        return *this;
    }

    boundary_iterator it(_trusted_memory);
    void* prev_block = nullptr;

    while (it._occupied_ptr && it._occupied_ptr < _occupied_ptr) {
        prev_block = it._occupied_ptr;
        ++it;
    }

    _occupied_ptr = prev_block;
    if (_occupied_ptr) {
        _occupied = (*reinterpret_cast<size_t*>(_occupied_ptr) & (size_t(1)));
    }

    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)
{
    boundary_iterator tmp = *this;
    ++(*this);
    return tmp;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)
{
    boundary_iterator tmp = *this;
    --(*this);
    return tmp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    if (!_occupied_ptr) return 0;
    return *reinterpret_cast<size_t*>(_occupied_ptr) & ~(size_t(1));
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    return _occupied;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    return _occupied_ptr;
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()
        : _occupied_ptr(nullptr), _occupied(false), _trusted_memory(nullptr) {}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted)
        : _trusted_memory(trusted)
{
    if (trusted != nullptr) {
        _occupied = *reinterpret_cast<size_t *>(trusted) & (size_t(1));
    }
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    return _occupied_ptr;
}

inline std::mutex &allocator_boundary_tags::get_mutex() const
{
    return *reinterpret_cast<std::mutex*>(
            reinterpret_cast<uint8_t*>(_trusted_memory) +
            sizeof(logger*) +
            sizeof(std::pmr::memory_resource*) +
            sizeof(fit_mode) +
            sizeof(size_t));
}

size_t allocator_boundary_tags::get_total_size() const noexcept
{
    if (!_trusted_memory) return 0;
    return *reinterpret_cast<size_t*>(
            reinterpret_cast<uint8_t*>(_trusted_memory) +
            sizeof(logger*) +
            sizeof(std::pmr::memory_resource*) +
            sizeof(fit_mode));
}

allocator_with_fit_mode::fit_mode allocator_boundary_tags::get_fit_mode() const {
    return *reinterpret_cast<fit_mode*>(
            reinterpret_cast<uint8_t*>(_trusted_memory) +
            sizeof(logger*) +
            sizeof(std::pmr::memory_resource*));
}

void* allocator_boundary_tags::find_suitable_block(size_t required_size,
        allocator_with_fit_mode::fit_mode mode)
{
    switch (mode) {
        case allocator_with_fit_mode::fit_mode::first_fit:
            return get_first_fit(required_size);
        case allocator_with_fit_mode::fit_mode::the_best_fit:
            return get_best_fit(required_size);
        case allocator_with_fit_mode::fit_mode::the_worst_fit:
            return get_worst_fit(required_size);
        default:
            throw std::invalid_argument("Unknown fit mode");
    }
}

void* allocator_boundary_tags::get_first_fit(size_t size) {
    const size_t total_size = size + occupied_block_metadata_size;

    size_t allocator_size = get_total_size();

    char* heap_start = (char*)_trusted_memory + allocator_metadata_size;
    char* heap_end = heap_start + allocator_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    if (*first_block_ptr == nullptr) {
        if (heap_end - heap_start >= total_size) {
            return heap_start;
        }
        return nullptr;
    }

    size_t front_hole = (char*)(*first_block_ptr) - heap_start;
    if (front_hole >= total_size) {
        return heap_start;
    }

    void* current = *first_block_ptr;
    while (current != nullptr) {
        size_t current_size = *reinterpret_cast<size_t*>(current);
        void* next = *reinterpret_cast<void**>((char*)current + sizeof(size_t));
        char* current_end = (char*)current + occupied_block_metadata_size + current_size;

        if (next == nullptr) {
            size_t end_hole = heap_end - current_end;
            if (end_hole >= total_size) {
                return current_end;
            }
            break;
        }

        size_t middle_hole = (char*)next - current_end;
        if (middle_hole >= total_size) {
            return current_end;
        }

        current = next;
    }

    return nullptr;
}

void* allocator_boundary_tags::get_best_fit(size_t size) {
    const size_t total_size = size + occupied_block_metadata_size;

    size_t allocator_size = get_total_size();

    char* heap_start = (char*)_trusted_memory + allocator_metadata_size;
    char* heap_end = heap_start + allocator_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    void* best_pos = nullptr;
    size_t best_diff = SIZE_MAX;

    if (*first_block_ptr == nullptr) {
        if (heap_end - heap_start >= total_size) {
            return heap_start;
        }
        return nullptr;
    }

    size_t front_hole = (char*)(*first_block_ptr) - heap_start;
    if (front_hole >= total_size) {
        best_pos = heap_start;
        best_diff = front_hole - total_size;
    }

    void* current = *first_block_ptr;
    while (current != nullptr) {
        size_t current_size = *reinterpret_cast<size_t*>(current);
        void* next = *reinterpret_cast<void**>((char*)current + sizeof(size_t));
        char* current_end = (char*)current + occupied_block_metadata_size + current_size;

        size_t hole_size = (next != nullptr)
                           ? (char*)next - current_end
                           : heap_end - current_end;

        if (hole_size >= total_size) {
            size_t diff = hole_size - total_size;
            if (diff < best_diff) {
                best_pos = current_end;
                best_diff = diff;

                if (diff == 0) break;
            }
        }

        current = next;
    }

    return best_pos;
}

void* allocator_boundary_tags::get_worst_fit(size_t size) {
    const size_t total_size = size + occupied_block_metadata_size;

    size_t allocator_size = get_total_size();

    char* heap_start = (char*)_trusted_memory + allocator_metadata_size;
    char* heap_end = heap_start + allocator_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    void* worst_pos = nullptr;
    size_t worst_size = 0;

    if (*first_block_ptr == nullptr) {
        if (heap_end - heap_start >= total_size) {
            return heap_start;
        }
        return nullptr;
    }

    size_t front_hole = (char*)(*first_block_ptr) - heap_start;
    if (front_hole >= total_size) {
        worst_pos = heap_start;
        worst_size = front_hole;
    }

    void* current = *first_block_ptr;
    while (current != nullptr) {
        size_t current_size = *reinterpret_cast<size_t*>(current);
        void* next = *reinterpret_cast<void**>((char*)current + sizeof(size_t));
        char* current_end = (char*)current + occupied_block_metadata_size + current_size;

        size_t hole_size = (next != nullptr)
                           ? (char*)next - current_end
                           : heap_end - current_end;

        if (hole_size >= total_size && hole_size > worst_size) {
            worst_pos = current_end;
            worst_size = hole_size;
        }

        current = next;
    }

    return worst_pos;
}
