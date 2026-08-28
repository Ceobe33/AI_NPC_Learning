cmake_minimum_required(VERSION 3.10)
project(StabilityTraining LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 核心稳定性门禁：开启全警告(-Wall -Wextra) 并将警告视为错误(-Werror)
# 开启 -O2 优化，使多线程时序更接近真实生产环境
add_compile_options(-Wall -Wextra -Werror -O2)

# 引入 pthread 线程库支持
find_package(Threads REQUIRED)

add_executable(stability_test main.cpp)
target_link_libraries(stability_test PRIVATE Threads::Threads)
