# Md3 Create

用 **Md3 QML 界面** 新建 Material Design 3 Qt Quick 工程（对标 Qt Creator「新建项目」），并自动接入同级的 [QML_MD3](../QML_MD3) 组件库。

## 功能

- **多选编译器 / Kit**（扫描 Qt 根目录，或「添加」自定义前缀）
- **自动去重项目名**（`MyApp` → `MyApp2`…；可选覆盖同名目录）
- **把 Md3 库复制进项目**（默认 `vendor/QML_MD3`，或改为外部引用）
- 选择模板：`empty` / `basic` / `rail`
- 生成完整 CMake 工程 + 每个 Kit 一份 `CMakePresets`
构建生成工程示例：

```powershell
cmake --preset default          # 第一个选中的 Kit
cmake --preset 6.10.2-msvc2022_64
cmake --build --preset default
```

## 构建本向导

```powershell
cd D:\QML_MD3\md3-create
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64
cmake --build build
.\build\Md3Create.exe
```

或在 Qt Creator 中打开本目录的 `CMakeLists.txt`。

## 生成工程后

```powershell
cmake --preset default
cmake --build build
```
