# FANET-NS3 项目开发与配置全指南

本项目是一个基于 NS-3 (v3.30+) 和 Qt5 构建的无人机自组织网络 (FANET) 仿真平台。针对高动态拓扑下的 MMGPSR 等协议进行了深度定制。

## 🛠️ 一、 环境约束与准备

本工程在现代编译器环境（如 Ubuntu 24.04, GCC 13）下运行，由于 NS-3 老版本代码与新编译器/Python 版本的冲突，必须严格执行以下配置：

* **编译器**: GCC 13+ (需处理 Deprecated 警告)
* **Python**: Python 3.11 (用于 `waf` 构建系统，Python 3.12 已删除 `imp` 模块)
* **依赖库**: Qt5, NS-3.30+, GCC/G++

## ⚙️ 二、 系统环境配置 (.bashrc)

为了确保编译器能找到 NS-3 库且运行时不报错，请在 `~/.bashrc` 中添加以下环境变量：

```bash
# NS-3 源码目录绝对路径
export NS3DIR=$HOME/fanet-ns3-ss/ns-3
# NS-3 库版本前缀 (由编译生成的 libns3-dev-xxx 决定)
export NS3VER=ns3-dev
# 动态库运行时搜索路径 (解决 libns3-dev-core-optimized.so 找不到的问题)
export LD_LIBRARY_PATH=$NS3DIR/build/lib:$LD_LIBRARY_PATH
```
*执行 `source ~/.bashrc` 使其生效。*

## 🏗️ 三、 NS-3 底层重编指南

针对 Python 3.12 兼容性及 GCC 13 警告报错，必须按以下步骤编译 NS-3：

1.  **安装 Python 3.11**: `sudo apt install python3.11`
2.  **清理旧缓存**: `python3.11 ./waf distclean`
3.  **配置 (关键点)**: 
    使用 `CXXFLAGS` 屏蔽废弃语法警告，并显式禁用 `werror`：
    ```bash
    CXXFLAGS="-Wno-error=deprecated-declarations" python3.11 ./waf configure --build-profile=optimized --disable-werror
    ```
4.  **编译**: `python3.11 ./waf build`

## 📂 四、 项目结构与 .pro 文件修复

本项目采用 Qt 子目录结构，包含 `fanet/` (主程序) 和 `test/` (测试模块)。

### 1. 关键依赖补丁
必须在 `fanet/fanet.pro` 和 `test/test.pro` 中手动补齐级联依赖库，否则链接阶段会报 `undefined reference`：

**在两个 .pro 文件的 `LIBS` 段落中确保包含以下模块：**
```makefile
unix:!macx: LIBS += -L$$(NS3DIR)/build/lib/ \
                    -l$$(NS3VER)-core-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-network-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-internet-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-mpi-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-point-to-point-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-applications-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-stats-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-csma-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-bridge-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-internet-apps-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-antenna-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-spectrum-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-energy-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-buildings-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-virtual-net-device-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-fd-net-device-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-lte-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-lr-wpan-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-wifi-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-mobility-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-netanim-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-wave-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-aodv-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-olsr-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-propagation-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-traffic-control-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-location-service-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-gpsr-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-pagpsr-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-mmgpsr-$$NS3_LIB_POSTFIX \
                    -l$$(NS3VER)-flow-monitor-$$NS3_LIB_POSTFIX
```

## 🚀 五、 项目编译流程

在 `fanet-ns3-ss` 根目录下执行“核弹级”重构命令：

```bash
# 1. 彻底清除旧的 Makefile 缓存
rm -f Makefile fanet/Makefile test/Makefile .qmake.stash

# 2. 强制重新生成构建脚本 (Release 模式)
qmake ns3-first.pro CONFIG+=release

# 3. 编译
make
```

## 📈 六、 运行仿真

进入 `fanet` 目录运行：
```bash
./fanet --nodes=128 --time=200 --speed=5.8 --mobility=RWP --traffic-models="PING_PONG" --routing=MMGPSR
```

## 🔍 七、 故障排查 (Troubleshooting)

| 报错信息 | 根因分析 | 解决方案 |
| :--- | :--- | :--- |
| `ModuleNotFoundError: No module named 'imp'` | Python 3.12 不兼容 | 使用 `python3.11 ./waf` |
| `error: unary_function is deprecated` | GCC 13 严格检查 | 加上 `--disable-werror` 配置 |
| `cannot find -lns3-core-optimized` | 库前缀不匹配 | 检查 `NS3VER` 是否为 `ns3-dev` |
| `undefined reference to ...` | 缺少底层依赖 | 在 `.pro` 文件中补齐 `antenna`, `lte` 等库 |
| `error while loading shared libraries` | 加载器找不到动态库 | 在 `.bashrc` 中配置 `LD_LIBRARY_PATH` |

---