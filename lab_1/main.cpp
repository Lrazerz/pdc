#include <iostream>
#include <algorithm>
#include <cassert> 
#include <chrono>

extern "C" {
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
}


void process_memory(void *memptr, size_t elements_to_calculate) {
#ifdef MMX
    volatile uint8_t max_difference { 0 };
    __asm__ __volatile__ (

        "       movq    %1, %%r8         \n"
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

        "       movq    %%mm0, %%rax     \n"
        "       rol     $32, %%rax       \n"
        "       movq    %%rax, %%mm1     \n"
        "       pmaxub  %%mm1, %%mm0     \n"

        "       movq    %%mm0, %%rax     \n"
        "       rol     $8, %%rax        \n"
        "       movq    %%rax, %%mm1     \n"
        "       pmaxub  %%mm1, %%mm0     \n"

        "       movq    %%mm0, %%rax     \n"
        "       rol     $16, %%rax       \n"
        "       movq    %%rax, %%mm1     \n"
        "       pmaxub  %%mm1, %%mm0     \n"        

        "       movq    %%mm0, %%rax     \n"
        "       emms                     \n"
        "       movb    %%al, %0         \n"

        : "=r"(max_difference)
        : "r"(memptr), "r"(elements_to_calculate)
        : "%rax", "%r8", "%mm0", "%mm1", "%mm2"
    );

    std::cout << "Result: " << static_cast<uint32_t>(max_difference)
              << " Time needed: " 
              << " ns" << std::endl;

#elif SSE
    volatile static float differences[4];
    static constexpr uint8_t abs_mask[16]
    {   
        0xFF, 0xFF, 0xFF, 0x7F,
        0xFF, 0xFF, 0xFF, 0x7F,
        0xFF, 0xFF, 0xFF, 0x7F, 
        0xFF, 0xFF, 0xFF, 0x7F 
    };

    __asm__ __volatile__ (        
        "       movupd  (%2), %%xmm0        \n"
        "       xorpd   %%xmm3, %%xmm3      \n"
        
        "       movq    %1, %%rax           \n"
        "       xorq    %%rdx, %%rdx        \n"
        "       movq    $4, %%rbx           \n"
        "       div     %%rbx               \n"
        "       cmpq    $0, %%rdx           \n"
        "       jnz     loop                \n"
        "       decq    %%rax               \n"
        "loop:                              \n"
        "       movups  (%0), %%xmm1        \n"
        "       movups  4(%0), %%xmm2       \n"
        "       subps   %%xmm2, %%xmm1      \n"
        "       andps   %%xmm0, %%xmm1      \n"
        "       maxps   %%xmm1, %%xmm3      \n"
        "       addq    $16, %0             \n"
        "       decq    %%rax               \n"
        "       jnz     loop                \n"
        
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

        :
        : "r"(memptr), "r"(elements_to_calculate), "r"(abs_mask), "r"(differences)
        : "%rax", "%rbx", "%rdx", "%xmm0", "%xmm1", "%xmm2", "%xmm3"
    );


    std::cout << "Result: " << *std::max_element(std::begin(differences), std::end(differences))
              << " Time needed: " 
              << " ns" << std::endl;        

#else
    volatile uint8_t max_difference { 0 };
    __asm__ __volatile__ (
        "       movq    %1, %%r8         \n"
        "       movq    %2, %%r9         \n"
        "       decq    %%r9             \n"
        "       xorq    %%rdx, %%rdx     \n"
        "loop:                           \n"
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
        "       jnz     loop             \n"
        "       movb    %%dl, %0         \n"

        : "=r"(max_difference)
        : "r"(memptr), "r"(elements_to_calculate)
        : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9"
    );

    std::cout << "Result: " << static_cast<uint32_t>(max_difference)
            << " Time needed: " 
            << " ns" << std::endl;
#endif
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
        process_memory(mapped_ptr, array_len);
        ::munmap(mapped_ptr, array_len);
        ::close(file_descriptor);
    }
    std::exit(0);
} 
