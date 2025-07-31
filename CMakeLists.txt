cmake_minimum_required(VERSION 3.16)      # 3.30 可，但多数机器默认 3.16~3.27
project(ZincBromideBatterySOC_BIG CXX)

# ------------------------------------------------------------------------------
# 1) C++11 即可；若想用 14/17 可自行改
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2) 可执行文件，包含核心逻辑 + 权重 + main()
add_executable(ZincBromideBatterySOC_BIG
        hybrid_cc_lstm_ekf_soc_v3_ac5.cpp
        weights.cpp
        hybrid_cc_lstm_ekf_soc.h          # <–– 别漏！
)

# 3) 关闭 RTTI / 异常（与固件设置保持一致；桌面调试也能提前发现类型问题）
target_compile_options(ZincBromideBatterySOC_BIG PRIVATE
        -fno-rtti
        -fno-exceptions
)

# 4) 打开 main() 测试入口
target_compile_definitions(ZincBromideBatterySOC_BIG PRIVATE DESKTOP_TEST)
#mkdir build && cd build
#cmake .. -DDESKTOP_TEST=ON
#cmake --build . -j
#./ZincBromideBatterySOC_BIG  ../20250327-80A-1H-3循环-2H-3循环-4H-1循环.soc.csv
