# ==============================================================================
# 工业级微型游戏构建 CMakeLists.txt (跨平台/体积极致优化版)
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

# 1. 项目基本信息
project(MiniChessGame 
        VERSION 1.0.0 
        LANGUAGES CXX)

# 2. 设置 C++ 工业级编译器规范与警告门禁（静态质量防错）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # Windows MSVC 编译器参数
    add_compile_options(/W4 /WX /Os /GL)
    add_link_options(/LTCG /INCREMENTAL:NO)
else()
    # GCC / Clang 编译器优化参数：开启全警告(-Wall -Wextra)，并针对体积进行极致优化(-Os)
    add_compile_options(-Wall -Wextra -Werror -Os)
    
    # 链接期极致压缩参数：-s (Strip) 剔除所有符号表与调试信息，最小化二进制包体
    add_link_options(-s)
endif()

# 3. 添加可执行文件目标
add_executable(mini_chess_game virtual.cpp)

# 4. 技术前置优化：开启链接时间优化（LTO / IPO）进一步压缩代码体积
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
  set_property(TARGET mini_chess_game PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    #message(STATUS "🚀 成功开启链接期全局优化 (LTO/IPO)，包体进一步压缩。")
else()
    message(STATUS "⚠️ 当前环境不支持 LTO: ${error}")
endif()

# 5. 自定义快捷运行指令 (相当于 make run)
add_custom_target(run
    COMMAND $<TARGET_FILE:mini_chess_game>
    DEPENDS mini_chess_game
    #COMMENT "🎮 正在启动本地微型 AI 象棋小游戏..."
)

# 6. 构建完成后的高亮提示日志
add_custom_command(TARGET mini_chess_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "=================================================="
    COMMAND ${CMAKE_COMMAND} -E echo "🎉 CMake 配置与编译成功！"
    COMMAND ${CMAKE_COMMAND} -E echo "👉 运行游戏请在 build 目录下执行: cmake --build . --target run"
    COMMAND ${CMAKE_COMMAND} -E echo "=================================================="
)
