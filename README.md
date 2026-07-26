# Md3 Create

用 **Md3 QML 界面** 新建 Material Design 3 Qt Quick 工程（对标 Qt Creator「新建项目」），并接入 [QML_MD3](https://github.com/wuyijing-dev/QML_MD3) 组件库（路径由 `MD3_ROOT` 指定）。

## 功能

- **多选编译器 / Kit**（扫描 Qt 根目录，或「添加」自定义前缀）
- **自动去重项目名**（`MyApp` → `MyApp2`…；可选覆盖同名目录）
- **复制预编译 Md3 进项目**（固定路径：工程同目录 `Md3/`；向导旁也放同名 `Md3/` 包）
- 选择模板：`empty` / `basic` / `rail`
- 生成完整 CMake 工程 + 每个 Kit 一份 `CMakePresets`

**发行布局（固定）：**

```text
Md3Create.exe
Md3\                 ← scripts/package-windows.ps1 产物（与向导同目录）
```

新建工程后：

```text
MyApp\
  CMakeLists.txt
  Main.qml
  Md3\               ← 从向导旁的 Md3 复制而来（固定相对路径）
```


构建生成工程示例：

```powershell
cmake --preset default          # 第一个选中的 Kit
cmake --preset 6.10.2-msvc2022_64
cmake --build --preset default
```

## 构建本向导

需要本机已有 [QML_MD3](https://github.com/wuyijing-dev/QML_MD3) 源码（路径用 `-DMD3_ROOT` 指定，不必与本仓库同级）。

```bash
# Linux / macOS
cmake -S . -B build -G Ninja \
  -DMD3_ROOT="$HOME/path/to/QML_MD3" \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.10.2/gcc_64"   # 按本机 Qt 改；系统包可省略
cmake --build build -j"$(nproc)"
./build/Md3Create
```

```powershell
# Windows
cmake -S . -B build -G Ninja `
  -DMD3_ROOT="D:/QML_MD3/QML_MD3" `
  -DCMAKE_PREFIX_PATH="D:/Qt/6.10.2/mingw_64"
cmake --build build
.\build\Md3Create.exe
```

未指定 `MD3_ROOT` 时会尝试常见位置（同级 `../QML_MD3`、`$MD3_ROOT` 环境变量、`~/QML_MD3` 等）。

或在 Qt Creator 中打开本目录的 `CMakeLists.txt`，在 CMake 变量里设置 `MD3_ROOT`。

## 生成工程后

```powershell
cmake --preset default
cmake --build build
```
