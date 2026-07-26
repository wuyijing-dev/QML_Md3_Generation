# Md3 Create

用 **Md3 QML 界面** 新建 Material Design 3 Qt Quick 工程。

CMake **优先**使用与本仓库 `CMakeLists.txt` **同目录**下的预编译包 `Md3/`（固定目录名）。

组件库打包说明见上游文档：  
https://github.com/wuyijing-dev/QML_MD3/blob/main/docs/packaging.md

## 推荐布局（固定）

```text
QML_Md3_Generation/          ← 本仓库源码
  CMakeLists.txt
  Main.qml
  projectgenerator.cpp
  Md3/                       ← 同目录预编译库（固定名，不要放进 build/）
    lib/
    include/
    lib/cmake/Md3/
  build/
    bin/Md3Create            ← 可执行文件
```

生成的工程同样是「CMake 与 Md3 同目录」：

```text
MyApp/
  CMakeLists.txt             ← 固定查找 ${CMAKE_CURRENT_SOURCE_DIR}/Md3
  Main.qml
  main.cpp                   ← 含 Q_IMPORT_QML_PLUGIN(Md3Plugin)
  Md3/                       ← 向导从旁侧 Md3/ 复制而来
```

## 构建本向导

### 1) 打包组件库

```bash
cd /path/to/QML_MD3
./scripts/package-linux.sh          # → dist/Md3
# Windows: .\scripts\package-windows.ps1
```

### 2) 拷到向导源码同目录

```bash
cd /path/to/QML_Md3_Generation
cp -a /path/to/QML_MD3/dist/Md3 ./Md3
ls Md3/lib    # 应有 libMd3.a / Md3.lib
```

### 3) 配置并编译

必须先 **configure**，再 **build**（空 `build/` 目录不够）：

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"
./build/bin/Md3Create
```

没有同目录 `Md3/` 时，可回退源码树：

```bash
cmake -S . -B build -G Ninja -DMD3_ROOT=/path/to/QML_MD3
cmake --build build -j"$(nproc)"
./build/bin/Md3Create
```

## 功能

- 多选编译器 / Kit（Linux 自动扫 `$HOME/Qt`、`/opt/Qt`、`/usr`、`qmake6` / `CMAKE_PREFIX_PATH`）
- 自动去重项目名
- 生成工程时把 `Md3/` 复制进新项目同目录
- 模板：`empty` / `basic` / `rail`

## 生成工程构建

```bash
cd /path/to/MyApp
cmake --preset default
cmake --build --preset default
```

## 常见问题

| 现象 | 处理 |
|------|------|
| 未找到编译器 / Kit | Linux：默认扫 `$HOME/Qt`、`/opt/Qt`、`/usr` 与 PATH 上的 `qmake6`；或点「添加」选 `~/Qt/6.x/gcc_64`。也可设 `QT_ROOT` / `CMAKE_PREFIX_PATH` |
| `not a CMake build directory` | 先执行 `cmake -S . -B build`，不要只 `mkdir build` |
| `Md3Create: Is a directory` | 删掉 `build/Md3Create/`；可执行文件在 `build/bin/Md3Create` |
| `module "Md3" is not installed` | 确认已 `git pull`（含 `Q_IMPORT_QML_PLUGIN`）；整库 `--whole-archive` 链接静态插件 |
| `undefined reference` KWindowEffects | `sudo apt install libkf6windowsystem-dev` 后重新 configure |
| `git pull` 卡住 | 改用 HTTPS：`git remote set-url origin https://github.com/wuyijing-dev/QML_Md3_Generation.git` |
| 把库拷进 `build/Md3` | 错误；必须放在**源码**旁的 `./Md3` |

## 相关链接

- 组件库：https://github.com/wuyijing-dev/QML_MD3  
- 打包文档：https://github.com/wuyijing-dev/QML_MD3/blob/main/docs/packaging.md  
- 集成文档：https://github.com/wuyijing-dev/QML_MD3/blob/main/docs/integration.md  
