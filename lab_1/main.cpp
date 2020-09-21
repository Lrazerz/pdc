#include <iostream>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert> 
#include <chrono>


void process_memory(void *memptr, size_t bytes_to_calculate) {
    uint8_t max_difference = 0;
    auto begin = std::chrono::high_resolution_clock::now();
    __asm__ __volatile__ (
#ifdef MMX
        "       movq    %1, %%r8        \n"
        "       movq    (%%r8), %%mm0   \n"
        "       movq    1(%%r8), %%mm1  \n"
        "       movq    %%mm0, %%mm2    \n"
        "       psubusb %%mm1, %%mm0    \n"
        "       psubusb %%mm2, %%mm1    \n"
        "       por     %%mm1, %%mm0    \n"

        "       movq    8(%%r8), %%mm1  \n"
        "       movq    9(%%r8), %%mm2  \n"
        "       movq    %%mm1, %%mm3    \n"
        "       psubusb %%mm2, %%mm1    \n"
        "       psubusb %%mm3, %%mm2    \n"
        "       por     %%mm2, %%mm1    \n"
        "       pmaxub  %%mm1, %%mm0    \n"

        "       movq    16(%%r8), %%mm1 \n"
        "       movq    17(%%r8), %%mm2 \n"
        "       movq    %%mm1, %%mm3    \n"
        "       psubusb %%mm2, %%mm1    \n"
        "       psubusb %%mm3, %%mm2    \n"
        "       por     %%mm2, %%mm1    \n"
        "       pmaxub  %%mm1, %%mm0    \n"

        "       movq    %%mm0, %%rax    \n"
        "       rol     $32, %%rax      \n"
        "       movd    %%rax, %%mm1    \n"
        "       pmaxub  %%mm1, %%mm0    \n"

        "       movq    %%mm0, %%rax    \n"
        "       rol     $8, %%rax       \n"
        "       movd    %%rax, %%mm1    \n"
        "       pmaxub  %%mm1, %%mm0    \n"

        "       movq    %%mm0, %%rax    \n"
        "       rol     $16, %%rax      \n"
        "       movd    %%rax, %%mm1    \n"
        "       pmaxub  %%mm1, %%mm0    \n"        

        "       movq    %%mm0, %%rax    \n"
        "       emms                    \n"
        "       movb    %%al, %0        \n"

        : "=r"(max_difference)
        : "r"(memptr), "r"(bytes_to_calculate)
        : "%rax", "%r8", "%mm0", "%mm1", "%mm2"

#else
        "       movq    %1, %%r8        \n"
        "       movq    %2, %%r9        \n"
        "       decq    %%r9            \n"
        "       xorq    %%rdx, %%rdx    \n"
        "loop:                          \n"
        "       movb    (%%r8), %%al    \n"
        "       movb    1(%%r8), %%bl   \n"
        "       subb    %%al, %%bl      \n"
        "       jnc     wr_diff         \n"
        "       movb    %%bl, %%al      \n"
        "       sarb    $7, %%al        \n" 
        "       xorb    %%al, %%bl      \n"
        "       subb    %%al, %%bl      \n"
        "wr_diff:                       \n"
        "       cmp     %%bl, %%dl      \n"
        "       jnb     cond            \n"
        "       movb    %%bl, %%dl      \n"
        "cond:                          \n"
        "       incq    %%r8            \n"
        "       decq    %%r9            \n"
        "       jnz     loop            \n"
        "       movb    %%dl, %0       \n"

        : "=r"(max_difference)
        : "r"(memptr), "r"(bytes_to_calculate)
        : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9"
#endif //MMX
    );
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Result: " << (uint16_t)max_difference
              << " Time needed: " 
              << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() 
              << " ns" << std::endl;
}


int main(int argc, char *argv[]) { 
    if(argc != 2 ) {
        std::cerr << "error: no input file" <<std::endl;
        std::exit(1);
    }
    constexpr ssize_t array_len = 25;
    auto file_descriptor = ::open(argv[1], O_RDWR);
    if (file_descriptor == -1) {
        std::cerr << "error! ::open()" << std::endl;
        std::exit(-2);
    }
    auto file_size = ::lseek(file_descriptor, 0, SEEK_END);
    if (file_size == -1) {
        std::cerr << "error! ::lseek()" << std::endl;
        std::exit(-3);
    }
    assert(file_size >= array_len);
    auto mapped_ptr = ::mmap(nullptr,
                            file_size,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            file_descriptor,
                            0);
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
