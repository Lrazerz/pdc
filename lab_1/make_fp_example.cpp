#include <fstream>
#include <iostream>

int main() {
    float array[25] = {0.6f, 34.1f, 40.0f, 44.1f, 50.0f,
                       54.1f,  60.0f, 64.1f, 70.0f, 74.1f,
                       80.0f,  84.1f, 90.0f, 94.1f, 100.0f,
                       104.1f, 110.0f, 114.1f, 120.0f, 124.1f,
                       134.1f, 154.6f, 164.1f, 174.1f, 180.0f};
    std::cout << sizeof(array) << std::endl;
    std::ofstream st {"example_float", std::ios::out | std::ios::binary};
    st.write(reinterpret_cast<char*>(&array), sizeof(array));
    return 0;
}
