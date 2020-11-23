#include <iostream>
#include <tuple>
#include <cmath>
#include <climits>

#ifdef LONGNUM

#include <boost/multiprecision/cpp_int.hpp>

namespace mp = boost::multiprecision;

#endif //LONGNUM


template<typename T>
constexpr size_t
bit_size() {
    return sizeof(T) * CHAR_BIT;
}


template <typename Tnum>
std::tuple<Tnum, uint64_t>
binary_modular_multiplication(Tnum b, const Tnum &n, const Tnum &m) {
    uint32_t high {}, low {};
    __asm__ __volatile__ (
    "       rdtsc                       \n"
    "       movl    %%edx, %0           \n"
    "       movl    %%eax, %1           \n"

    : "=r"(high), "=r"(low)
    :: "%rax", "%rbx", "%rcx", "%rdx"
    );
    uint64_t ticks {
        (static_cast<uint64_t>(high) << bit_size<uint32_t>()) | low
    };

    Tnum y2;
    Tnum y3;
    Tnum result;

#pragma omp parallel sections num_threads(2)
    {
#pragma omp section
        y2 = b % m;
#pragma omp section 
        {
            y3 = (n % 2 == 0) ? 1 : b;
            result = y3 % m;
        }     
    }

    b = y2 * y2;
    y2 = b % m;

    for(auto i = n >> 1U; i != 0; i >>= 1U) 
#pragma omp parallel sections num_threads(2) 
    {
#pragma omp section
        {
            y3 = ((i & 0x1U) == 0x1) ? result * y2 : result;
            result = y3 % m;
        }
#pragma omp section
        {
            b = y2 * y2;
            y2 = b % m;
        }
    }

    __asm__ __volatile__ (
    "       rdtsc                       \n"
    "       movl    %%edx, %0           \n"
    "       movl    %%eax, %1           \n"
    : "=r"(high), "=r"(low)
    :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    return std::make_tuple(
        result,
        ((static_cast<uint64_t>(high) << bit_size<uint32_t>()) | low) - ticks
    );
}


template <typename Tnum>
std::tuple<Tnum, uint64_t>
montgomery_modular_multiplication(Tnum b, const Tnum n, const Tnum m) {
    int sigbit_count = 0;
    while(n >> sigbit_count != 0x1U) {
        ++sigbit_count;
    }

    uint32_t high, low;
    __asm__ __volatile__ (
    "       rdtsc                       \n"
    "       movl    %%edx, %0           \n"
    "       movl    %%eax, %1           \n"

    : "=r"(high), "=r"(low)
    :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    uint64_t ticks {
        (static_cast<uint64_t>(high) << bit_size<uint32_t>()) | low
    };

    Tnum y11 { b % m };
    Tnum y2  { b * b};
    Tnum y22 { y2 % m };

#pragma omp parallel sections num_threads(2)
    {
#pragma omp section
        y11 = b % m;
#pragma omp section
        {
            y2 = b * b;
            y22 = y2 % m;
        }
    }    

    for(auto i = sigbit_count - 1; i >= 0; --i) {
#pragma omp parallel sections num_threads(2)
        {
#pragma omp section
            b = ((n >> i & 0x1U) == 0x1U) ? y11 * y22 : y11 * y11;
#pragma omp section
            y2 = ((n >> i & 0x1U) == 0x1U) ? y22 * y22 : y22 * y11;
        }
#pragma omp parallel sections num_threads(2)
        {
#pragma omp section
            y11 = b % m;
#pragma omp section
            y22 = y2 % m;
        }
    }

    __asm__ __volatile__ (
    "       rdtsc                       \n"
    "       movl    %%edx, %0           \n"
    "       movl    %%eax, %1           \n"
    : "=r"(high), "=r"(low)
    :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    return std::make_tuple(
        y11,
        (static_cast<uint64_t>(high) << bit_size<uint32_t>() | low) - ticks
    );
}


template <typename Tnum>
std::tuple<Tnum, uint64_t>
stupid_modular_multiplication(Tnum b, const Tnum n, const Tnum m) {
    int sigbit_count = 0;
    while(n >> sigbit_count++ != 0x1U);

    uint32_t high, low;
    __asm__ __volatile__ (
    "       rdtsc                       \n"
    "       movl    %%edx, %0           \n"
    "       movl    %%eax, %1           \n"

    : "=r"(high), "=r"(low)
    :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    uint64_t ticks {
        (static_cast<uint64_t>(high) << bit_size<uint32_t>()) | low
    };
    bool is_one;
    Tnum d { 1 };
    while(--sigbit_count >= 0) {
#pragma omp parallel sections num_threads(2)
        {
#pragma omp section
            d = (d * d) % m;
#pragma omp section
            is_one = ((n >> sigbit_count & 0x1U) == 0x1U);
        }
        if(is_one) {
            d = (d * b) % m;
        }
    }

    __asm__ __volatile__ (
    "       rdtsc                       \n"
    "       movl    %%edx, %0           \n"
    "       movl    %%eax, %1           \n"
    : "=r"(high), "=r"(low)
    :: "%rax", "%rbx", "%rcx", "%rdx"
    );

    return std::make_tuple(
        d,
        (static_cast<uint64_t>(high) << bit_size<uint32_t>() | low) - ticks
    );
}


template<typename Tnum, typename Tfunc>
void
print_result(Tfunc func, Tnum &b, const Tnum &n, const Tnum &m) {
    constexpr int REPEATS { 100 };
    Tnum sum { 0 };
    for(int i = 0; i < REPEATS; ++i) {
        sum += std::get<1>(func(b, n, m));
    }
    std::cout << "b^n(mod m) = "
              << std::get<0>(func(b, n, m))
              << ", avg time - " << sum / REPEATS << std::endl;
}


int
main() {

#ifdef LONGNUM
    mp::uint1024_t b {
        "51646598761456029847918723690243598169283586498756982748577153254326542"
        "19823749876297691827369872430591702985798732495182705962394576536541018" };

    mp::uint1024_t n {
        "1512452348507882374659827964875687263409576823749857629837458765876"
        "1873264872058019283401032981098398762138746918234079817238758765876" };

    mp::uint1024_t m {
        "23346710934984823746598798726876423645902387465987198794854076536542"
        "23450198750918753747538476587364598762934875119465927346514565425426" };

#else
    uint64_t b { 7 };
    uint64_t n { 560 };
    uint64_t m { 561 };

#endif //LONGNUM

    print_result(binary_modular_multiplication<decltype(b)>, b, n, m);
    print_result(montgomery_modular_multiplication<decltype(b)>, b, n, m);
    print_result(stupid_modular_multiplication<decltype(b)>, b, n, m);
    return 0;
}
