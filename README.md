# Light Sensor 光照传感器组件

## 项目简介
提供基于 I2C 的光照传感器采集组件，封装初始化与轮询读取流程，减少对具体芯片寄存器操作的重复实现，便于快速接入光照传感器数据。

## 功能特性
支持：
- I2C 光照传感器接入
- W1160 传感器驱动与示例测试程序
- 轮询读取光照值（lux）

不支持：
- 暂不支持非I2C传感器（如 SPI/ADC）
- 传感器自动识别与热插拔
- 事件/中断触发模式

## 快速开始
最短路径跑起来：编译运行 `./test_light_sensor_i2c`，连接 W1160 后读取光照值。

### 环境准备
- Linux 系统（具备 I2C 设备节点，如 `/dev/i2c-*`）
- `gcc`/`cmake`/`make` 构建工具
- W1160 传感器硬件与正确的 I2C 连接
- 非root用户下，需要开启i2c节点的读写权限

### 构建编译
以下为脱离 SDK 的独立构建方式：
```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=ON
make -j
```
说明：
- W1160 依赖 `libstk_alps_lib.a`，默认路径为 `src/drivers/drv_i2c_w1160/libstk_alps_lib.a`。
- 如需自定义路径，可在 `cmake` 时指定 `-DSTK_ALPS_STATIC_LIB=/path/to/libstk_alps_lib.a`。

### 运行示例
```bash
sudo ./build/./test_light_sensor_i2c
```
代码示例可参考test/test_light_sensor_i2c.c



## 详细使用
保留，引用到后续的官方文档。

## 常见问题
- 读数一直失败：确认 I2C 设备节点与地址是否正确（默认 W1160 地址 `0x48`）。
- 无法访问 I2C：检查 `/dev/i2c-*` 权限，必要时使用 `sudo`。
- 链接失败：确认 `libstk_alps_lib.a` 路径正确或已通过 `STK_ALPS_STATIC_LIB` 指定。

## 版本与发布

| 版本   | 日期       | 说明 |
| ------ | ---------- | ---- |
| 1.0.0  | 2026-02-28 | 初始版本，提供 W1160 I2C 读取示例。 |

## 贡献方式

欢迎参与贡献：提交 Issue 反馈问题，或通过 Pull Request 提交代码。

- **编码规范**：本组件 C 代码遵循 [Google C++ 风格指南](https://google.github.io/styleguide/cppguide.html)（C 相关部分），请按该规范编写与修改代码。
- **提交前检查**：请在提交前运行本仓库的 lint 脚本，确保通过风格检查：
  ```bash
  # 在仓库根目录执行（检查全仓库）
  bash scripts/lint/lint_cpp.sh

  # 仅检查本组件
  bash scripts/lint/lint_cpp.sh components/peripherals/light_sensor
  ```
  脚本路径：`scripts/lint/lint_cpp.sh`。若未安装 `cpplint`，可先执行：`pip install cpplint` 或 `pipx install cpplint`。
- **提交说明**：提交 Issue 或 PR 前请描述传感器型号、I2C 连接与复现步骤。

## License
本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
