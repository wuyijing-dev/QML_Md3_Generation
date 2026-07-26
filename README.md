# Md3 Create

用 **Md3 QML 界面** 新建 Material Design 3 Qt Quick 工程。CMake **优先**使用与本仓库 `CMakeLists.txt` **同目录**下的预编译包 `Md3/`。

## 推荐布局（固定）

```text
QML_Md3_Generation/          ← 本仓库源码
  CMakeLists.txt
  Main.qml
  projectgenerator.cpp
  Md3/                       ← 同目录预编译库（固定名）
    lib/
    include/
    lib/cmake/Md3/
  build/                     ← 构建目录（不要把库放这里）
```

生成工程后同样是同目录 `./Md3`：

```text
MyApp/
  CMakeLists.txt
  Main.qml
  Md3/
```

## 构建本向导

### 1) 先打包组件库

```bash
cd /home/wyj/QML_MD3          # 你的库源码根
./scripts/package-linux.sh
# → /home/wyj/QML_MD3/dist/Md3
```

### 2) 拷到向导源码同目录（注意：不是 build/）

```bash
cd ~/md3_generation/QML_Md3_Generation
cp -a /home/wyj/QML_MD3/dist/Md3 ./Md3
```

### 3) 配置并编译（无需 -DMD3_ROOT）

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"
./build/bin/Md3Create
```

若暂时没有同目录 `Md3/`，可回退到源码：

```bash
cmake -S . -B build -G Ninja -DMD3_ROOT=/home/wyj/QML_MD3
cmake --build build -j"$(nproc)"
./build/bin/Md3Create
```
## 功能

- 多选编译器 / Kit
- 自动去重项目名
- 生成工程时把 `Md3/` 复制进新项目同目录
- 模板：`empty` / `basic` / `rail`

## 生成工程构建

```bash
cmake --preset default
cmake --build --preset default
```
