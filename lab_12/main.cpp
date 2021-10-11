#include <iostream>
#include <algorithm>
#include <functional>
#include <cassert> 
#include <tuple>

extern "C" {
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
}



std::tuple<uint64_t, int>
process_memory(void *memptr, size_t elements_to_calculate) {
    volatile uint8_t max_difference { 0 };
    uint32_t high, low;
    __asm__ __volatile__ (
        "       rdtsc                       \n"
        "       movl    %%edx, %0           \n"
        "       movl    %%eax, %1           \n"
        
        : "=r"(high), "=r"(low)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );
    uint64_t ticks = ((uint64_t)high << 32) | low;

    __asm__ __volatile__ (
        "       movq    %1, %%r8         \n"
        "       movq    %2, %%r9         \n"
        "       decq    %%r9             \n"
        "       xorq    %%rdx, %%rdx     \n"
        "lp:                             \n"
        "       movb    (%%r8), %%al     \n"
        "       movb    1(%%r8), %%bl    \n"
        "       subb    %%al, %%bl       \n"
        "       jnc     wr_diff          \n"
        "       movb    %%bl, %%al       \n"
        "       sarb    $7, %%al         \n" 
        "       xorb    %%al, %%bl       \n"
        "       subb    %%al, %%bl       \n"
        "wr_diff:                        \n"
        "       cmp     %%bl, %%dl       \n"
        "       jnb     cond             \n"
        "       movb    %%bl, %%dl       \n"
        "cond:                           \n"
        "       incq    %%r8             \n"
        "       decq    %%r9             \n"
        "       jnz     lp               \n"
        "       movb    %%dl, %0         \n"

        : "=r"(max_difference)
        : "r"(memptr), "r"(elements_to_calculate)
        : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9"
    );

    __asm__ __volatile__ (
        "       rdtsc                       \n"
        "       movl    %%edx, %0           \n"
        "       movl    %%eax, %1           \n"
        : "=r"(high), "=r"(low)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    return std::make_tuple(
        (((uint64_t)high << 32) | low) - ticks,
        static_cast<int>(max_difference)
    );
}



std::tuple<uint64_t, int>
process_memory_mmx(void *memptr, size_t elements_to_calculate) {
    volatile uint8_t differences[8];
    uint32_t high, low;
    __asm__ __volatile__ (
        "       rdtsc                       \n"
        "       movl    %%edx, %0           \n"
        "       movl    %%eax, %1           \n"
        
        : "=r"(high), "=r"(low)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );
    uint64_t ticks = ((uint64_t)high << 32) | low;
    __asm__ __volatile__ (

        "       movq    %0, %%r8         \n"
        "       movq    (%%r8), %%mm0    \n"
        "       movq    1(%%r8), %%mm1   \n"
        "       movq    %%mm0, %%mm2     \n"
        "       psubusb %%mm1, %%mm0     \n"
        "       psubusb %%mm2, %%mm1     \n"
        "       por     %%mm1, %%mm0     \n"

        "       movq    8(%%r8), %%mm1   \n"
        "       movq    9(%%r8), %%mm2   \n"
        "       movq    %%mm1, %%mm3     \n"
        "       psubusb %%mm2, %%mm1     \n"
        "       psubusb %%mm3, %%mm2     \n"
        "       por     %%mm2, %%mm1     \n"
        "       pmaxub  %%mm1, %%mm0     \n"

        "       movq    16(%%r8), %%mm1  \n"
        "       movq    17(%%r8), %%mm2  \n"
        "       movq    %%mm1, %%mm3     \n"
        "       psubusb %%mm2, %%mm1     \n"
        "       psubusb %%mm3, %%mm2     \n"
        "       por     %%mm2, %%mm1     \n"
        "       pmaxub  %%mm1, %%mm0     \n"
        "       movq    %%mm0, (%2)      \n"

        :: "r"(memptr), "r"(elements_to_calculate), "r"(differences)
        : "%rax", "%r8", "%mm0", "%mm1", "%mm2"
    );

    __asm__ __volatile__ (
        "       rdtsc                       \n"
        "       movl    %%edx, %0           \n"
        "       movl    %%eax, %1           \n"
        : "=r"(high), "=r"(low)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    return std::make_tuple(
        (((uint64_t)high << 32) | low) - ticks,
        static_cast<int>(*std::max_element(std::begin(differences), std::end(differences)))
    );
}



std::tuple<uint64_t, float>
process_memory_sse(void *memptr, size_t elements_to_calculate) {
    alignas(16) volatile float differences[4];
    alignas(16) static constexpr uint8_t abs_mask[16]
    {   
        0xFF, 0xFF, 0xFF, 0x7F,
        0xFF, 0xFF, 0xFF, 0x7F,
        0xFF, 0xFF, 0xFF, 0x7F, 
        0xFF, 0xFF, 0xFF, 0x7F 
    };

    uint32_t high, low;
    __asm__ __volatile__ (
        "       rdtsc                       \n"
        "       movl    %%edx, %0           \n"
        "       movl    %%eax, %1           \n"
        
        : "=r"(high), "=r"(low)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );
    uint64_t ticks = ((uint64_t)high << 32) | low;

    __asm__ __volatile__ (        
        "       movupd  (%2), %%xmm0        \n"
        "       xorpd   %%xmm3, %%xmm3      \n"
        
        "       movq    %1, %%rax           \n"
        "       xorq    %%rdx, %%rdx        \n"
        "       movq    $4, %%rbx           \n"
        "       div     %%rbx               \n"
        "       cmpq    $0, %%rdx           \n"
        "       jnz     loop_sse            \n"
        "       decq    %%rax               \n"
        
        "loop_sse:                          \n"
        "       movups  (%0), %%xmm1        \n"
        "       movups  4(%0), %%xmm2       \n"
        "       subps   %%xmm2, %%xmm1      \n"
        "       andps   %%xmm0, %%xmm1      \n"
        "       maxps   %%xmm1, %%xmm3      \n"
        "       addq    $16, %0             \n"
        "       decq    %%rax               \n"
        "       jnz     loop_sse            \n"
        
        "       cmpq    $1, %%rdx           \n"
        "       je      write_max           \n"
        
        "       subq    %%rdx, %0           \n"
        "       movups  (%0), %%xmm1        \n"
        "       movups  4(%0), %%xmm2       \n"
        "       subps   %%xmm2, %%xmm1      \n"
        "       andps   %%xmm0, %%xmm1      \n"
        "       maxps   %%xmm1, %%xmm3      \n"

        "write_max:                         \n"
        "       movups  %%xmm3, (%3)        \n"

        :: "r"(memptr), "r"(elements_to_calculate), "r"(abs_mask), "r"(differences)
        : "%rax", "%rbx", "%rdx", "%xmm0", "%xmm1", "%xmm2", "%xmm3"
    );

    __asm__ __volatile__ (
        "       rdtsc                       \n"
        "       movl    %%edx, %0           \n"
        "       movl    %%eax, %1           \n"
        : "=r"(high), "=r"(low)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    return std::make_tuple(
        (((uint64_t)high << 32) | low) - ticks,
        *std::max_element(std::begin(differences), std::end(differences))
    );
}



template <typename T>
void print_result(T func, void *mapped_ptr, size_t array_len) {
    constexpr int REPEATS { 100 };
    uint64_t sum { 0 };
    for(int i = 0; i < REPEATS; ++i) {
        sum += std::get<0>(func(mapped_ptr, array_len));
    }
    std::cout << "Result: " << std::get<1>(func(mapped_ptr, array_len))
              << " avg time - " << sum / REPEATS << std::endl;   
}



int main(int argc, char *argv[]) { 
    if(argc != 2 ) {
        std::cerr << "error: no input file" <<std::endl;
        std::exit(1);
    }

    constexpr off_t array_len { 25 };
    auto file_descriptor { ::open(argv[1], O_RDONLY) };
    if (file_descriptor == -1) {
        std::cerr << "error! ::open()" << std::endl;
        std::exit(-2);
    }
    auto file_size { ::lseek(file_descriptor, 0, SEEK_END) };
    if (file_size == -1) {
        std::cerr << "error! ::lseek()" << std::endl;
        std::exit(-3);
    }
    assert(file_size >= array_len);
    auto mapped_ptr { ::mmap(nullptr,
                            file_size,
                            PROT_READ,
                            MAP_SHARED,
                            file_descriptor,
                            0) };
    if(mapped_ptr == MAP_FAILED) {
        std::cerr << "mapping error" << std::endl;
        ::close(file_descriptor);
    }
    else {
        
#ifdef MMX
        print_result(process_memory_mmx, mapped_ptr, array_len); 
#elif SSE
        print_result(process_memory_sse, mapped_ptr, array_len);
#else
        print_result(process_memory, mapped_ptr, array_len);
#endif //MMX       
        ::munmap(mapped_ptr, array_len);
        ::close(file_descriptor);
    }
    std::exit(0);
} 
