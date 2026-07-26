#include "projectgenerator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

ProjectGenerator::ProjectGenerator(QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_WIN)
    m_qtRoot = QStringLiteral("D:/Qt");
#else
    m_qtRoot = QStringLiteral("/opt/Qt");
#endif
    // Sibling of md3-create: ../QML_MD3
    const QDir appDir(QCoreApplication::applicationDirPath());
    QString guess = QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("../../QML_MD3")));
    if (!QFileInfo::exists(guess + QStringLiteral("/CMakeLists.txt")))
        guess = QDir::cleanPath(QDir(QStringLiteral(__FILE__)).absoluteFilePath(QStringLiteral("../QML_MD3")));
    // Source-tree relative when running from build/
    const QString fromSource = QDir::cleanPath(
        QFileInfo(QStringLiteral(__FILE__)).absoluteDir().absoluteFilePath(QStringLiteral("../QML_MD3")));
    if (QFileInfo::exists(fromSource + QStringLiteral("/CMakeLists.txt")))
        guess = fromSource;
    m_md3Path = guess;
    refreshKits();
}

void ProjectGenerator::setQtRoot(const QString &path)
{
    if (m_qtRoot == path)
        return;
    m_qtRoot = path;
    emit qtRootChanged();
    refreshKits();
}

void ProjectGenerator::setMd3Path(const QString &path)
{
    if (m_md3Path == path)
        return;
    m_md3Path = path;
    emit md3PathChanged();
}

bool ProjectGenerator::isValidMd3Path(const QString &path) const
{
    if (path.trimmed().isEmpty())
        return false;
    const QString p = QDir::cleanPath(path);
    if (QFileInfo::exists(p + QStringLiteral("/CMakeLists.txt")))
        return true;
    if (QFileInfo::exists(p + QStringLiteral("/src/Md3/CMakeLists.txt")))
        return true;
    // Prebuilt build tree (contains libMd3)
    if (!findPrebuiltMd3Dir(p).isEmpty())
        return true;
    return QFileInfo::exists(p + QStringLiteral("/../CMakeLists.txt"))
            && QFileInfo(p).fileName() == QLatin1String("Md3");
}

QString ProjectGenerator::findPrebuiltMd3Dir(const QString &path) const
{
    const QString p = QDir::cleanPath(path);
    auto hasLib = [](const QString &dir) {
        return QFileInfo::exists(dir + QStringLiteral("/libMd3.a"))
                || QFileInfo::exists(dir + QStringLiteral("/libMd3.lib"))
                || QFileInfo::exists(dir + QStringLiteral("/Md3.lib"));
    };

    QStringList candidates;
    candidates << p
               << p + QStringLiteral("/src/Md3")
               << p + QStringLiteral("/build-lib/src/Md3")
               << p + QStringLiteral("/build/src/Md3")
               << p + QStringLiteral("/build-mingw/src/Md3")
               << p + QStringLiteral("/build-mingw/md3/src/Md3");

    // Sibling md3-create build (common local layout)
    const QString siblingCreate = QDir(p + QStringLiteral("/../md3-create")).absolutePath();
    candidates << siblingCreate + QStringLiteral("/build-mingw/md3/src/Md3")
               << siblingCreate + QStringLiteral("/build/md3/src/Md3");

    // Walk one level of build* under repo for */src/Md3/libMd3.*
    QDir root(p);
    if (root.exists()) {
        const QStringList builds = root.entryList(QStringList{QStringLiteral("build*")},
                                                  QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &b : builds) {
            candidates << root.filePath(b + QStringLiteral("/src/Md3"));
            candidates << root.filePath(b + QStringLiteral("/md3/src/Md3"));
        }
    }

    for (const QString &c : candidates) {
        const QString abs = QDir::cleanPath(c);
        if (hasLib(abs))
            return abs;
    }
    return {};
}

bool ProjectGenerator::projectExists(const QString &outputDir, const QString &name) const
{
    if (outputDir.trimmed().isEmpty() || name.trimmed().isEmpty())
        return false;
    const QString dest = QDir(outputDir).filePath(name.trimmed());
    QDir d(dest);
    if (!d.exists())
        return false;
    return !d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

QString ProjectGenerator::uniqueProjectName(const QString &outputDir, const QString &baseName) const
{
    QString base = baseName.trimmed();
    static const QRegularExpression nameRe(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9_]*$)"));
    if (!nameRe.match(base).hasMatch())
        base = QStringLiteral("Md3App");

    if (!projectExists(outputDir, base))
        return base;

    // Strip trailing digits for better series: MyApp2 -> MyApp + 3
    QString stem = base;
    QRegularExpression trailing(QStringLiteral(R"(^(.*?)(\d+)$)"));
    const QRegularExpressionMatch m = trailing.match(base);
    int start = 2;
    if (m.hasMatch()) {
        stem = m.captured(1);
        if (stem.isEmpty())
            stem = base;
        start = m.captured(2).toInt() + 1;
    }

    for (int n = start; n < 10000; ++n) {
        const QString candidate = stem + QString::number(n);
        if (!projectExists(outputDir, candidate))
            return candidate;
    }
    return stem + QStringLiteral("_") + QString::number(QDateTime::currentSecsSinceEpoch());
}

QString ProjectGenerator::suggestProjectName(const QString &outputDir, const QString &desiredName) const
{
    QString base = desiredName.trimmed();
    if (base.isEmpty())
        base = QStringLiteral("MyMd3App");
    // Sanitize lightly
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
    if (base.isEmpty() || !base[0].isLetter())
        base = QStringLiteral("App_") + base;
    return uniqueProjectName(outputDir, base);
}

static QString normalizeMd3Root(const QString &path)
{
    const QString p = QDir::cleanPath(path);
    if (QFileInfo::exists(p + QStringLiteral("/CMakeLists.txt")))
        return p;
    if (QFileInfo::exists(p + QStringLiteral("/src/Md3/CMakeLists.txt")))
        return p; // repo root preferred for add_subdirectory of whole repo
    // If user selected src/Md3, go up to repo when possible
    QDir d(p);
    if (d.dirName() == QLatin1String("Md3") && d.cdUp() && d.cdUp()
            && QFileInfo::exists(d.absolutePath() + QStringLiteral("/CMakeLists.txt")))
        return d.absolutePath();
    return p;
}

bool ProjectGenerator::addCustomKit(const QString &prefixPath)
{
    const QString prefix = cmakePath(prefixPath);
    if (prefix.isEmpty() || !QFileInfo::exists(prefix)) {
        setError(QStringLiteral("Kit 路径无效"));
        return false;
    }
    const QString cfg = prefix + QStringLiteral("/lib/cmake/Qt6/Qt6Config.cmake");
    const QString qmake = prefix + QStringLiteral("/bin/qmake")
#if defined(Q_OS_WIN)
            + QStringLiteral(".exe")
#endif
            ;
    if (!QFileInfo::exists(cfg) && !QFileInfo::exists(qmake)) {
        setError(QStringLiteral("不是有效的 Qt Kit 前缀: %1").arg(prefix));
        return false;
    }

    // Infer version / kit from path .../6.10.2/mingw_64
    QDir kitDir(prefix);
    const QString kitName = kitDir.dirName();
    QString ver = QStringLiteral("custom");
    if (kitDir.cdUp())
        ver = kitDir.dirName();

    for (const QVariant &v : m_kits) {
        if (v.toMap().value(QStringLiteral("prefix")).toString() == prefix)
            return true; // already present
    }

    QVariantMap row;
    row.insert(QStringLiteral("label"), QStringLiteral("Qt %1 / %2").arg(ver, kitName));
    row.insert(QStringLiteral("version"), ver);
    row.insert(QStringLiteral("kit"), kitName);
    row.insert(QStringLiteral("prefix"), prefix);
    row.insert(QStringLiteral("custom"), true);
    m_kits.append(row);
    emit kitsChanged();
    return true;
}

void ProjectGenerator::setError(const QString &msg)
{
    m_lastError = msg;
    emit lastErrorChanged();
    emit failed(msg);
}

void ProjectGenerator::setBusy(bool on)
{
    if (m_busy == on)
        return;
    m_busy = on;
    emit busyChanged();
}

QString ProjectGenerator::cmakePath(const QString &native)
{
    QString p = QDir::fromNativeSeparators(QFileInfo(native).absoluteFilePath());
    return p;
}

QString ProjectGenerator::render(const QString &text, const QMap<QString, QString> &vars)
{
    QString out = text;
    for (auto it = vars.constBegin(); it != vars.constEnd(); ++it)
        out.replace(QStringLiteral("{{") + it.key() + QStringLiteral("}}"), it.value());
    return out;
}

void ProjectGenerator::refreshKits()
{
    m_kits.clear();
    QDir root(m_qtRoot);
    if (!root.exists()) {
        emit kitsChanged();
        return;
    }

    const QStringList versions = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    static const QRegularExpression verRe(QStringLiteral(R"(^\d+\.\d+(\.\d+)?$)"));
    for (const QString &ver : versions) {
        if (!verRe.match(ver).hasMatch())
            continue;
        QDir verDir(root.filePath(ver));
        const QStringList kits = verDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &kit : kits) {
            const QString prefix = verDir.filePath(kit);
            const QString cfg = prefix + QStringLiteral("/lib/cmake/Qt6/Qt6Config.cmake");
            const QString qmake = prefix + QStringLiteral("/bin/qmake")
#if defined(Q_OS_WIN)
                + QStringLiteral(".exe")
#endif
                ;
            if (!QFileInfo::exists(cfg) && !QFileInfo::exists(qmake))
                continue;
            QVariantMap row;
            row.insert(QStringLiteral("label"), QStringLiteral("Qt %1 / %2").arg(ver, kit));
            row.insert(QStringLiteral("version"), ver);
            row.insert(QStringLiteral("kit"), kit);
            row.insert(QStringLiteral("prefix"), cmakePath(prefix));
            m_kits.append(row);
        }
    }
    emit kitsChanged();
}

QString ProjectGenerator::pickDirectory(const QString &title, const QString &startDir)
{
    QWidget *parent = QApplication::activeWindow();
    const QString start = startDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : startDir;
    return QFileDialog::getExistingDirectory(parent, title, start);
}

bool ProjectGenerator::writeRendered(const QString &qrcPath, const QString &destFile,
                                     const QMap<QString, QString> &vars)
{
    QFile in(qrcPath);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(QStringLiteral("缺少模板: %1").arg(qrcPath));
        return false;
    }
    const QString rendered = render(QString::fromUtf8(in.readAll()), vars);
    QFileInfo fi(destFile);
    QDir().mkpath(fi.absolutePath());
    QFile out(destFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        setError(QStringLiteral("无法写入: %1").arg(destFile));
        return false;
    }
    out.write(rendered.toUtf8());
    return true;
}

bool ProjectGenerator::writeBytes(const QString &qrcPath, const QString &destFile)
{
    QFile in(qrcPath);
    if (!in.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("缺少模板: %1").arg(qrcPath));
        return false;
    }
    QFileInfo fi(destFile);
    QDir().mkpath(fi.absolutePath());
    QFile out(destFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("无法写入: %1").arg(destFile));
        return false;
    }
    out.write(in.readAll());
    return true;
}

bool ProjectGenerator::copyOneFile(const QString &src, const QString &dst)
{
    QDir().mkpath(QFileInfo(dst).absolutePath());
    if (QFile::exists(dst))
        QFile::remove(dst);
    return QFile::copy(src, dst);
}

bool ProjectGenerator::copyPrebuiltMd3(const QString &sourceOrBuildPath, const QString &destVendorDir)
{
    const QString buildMd3 = findPrebuiltMd3Dir(sourceOrBuildPath);
    if (buildMd3.isEmpty()) {
        setError(QStringLiteral(
            "未找到预编译 libMd3（.a/.lib）。请先编译 Md3，或选择含 libMd3.a 的构建目录"
            "（例如 md3-create/build-mingw/md3/src/Md3）。"));
        return false;
    }

    // Headers come from source tree next to the build, or from the given source path.
    QString srcRoot = normalizeMd3Root(sourceOrBuildPath);
    auto hasMd3Headers = [](const QString &root) {
        return QFileInfo::exists(root + QStringLiteral("/src/Md3/md3.h"))
                || QFileInfo::exists(root + QStringLiteral("/md3.h"));
    };
    if (!hasMd3Headers(srcRoot)) {
        QDir d(buildMd3);
        for (int i = 0; i < 8 && d.cdUp(); ++i) {
            const QString abs = d.absolutePath();
            if (hasMd3Headers(abs)) {
                srcRoot = abs;
                break;
            }
            // Sibling checkout: .../QML_MD3/QML_MD3 next to md3-create
            const QString nested = abs + QStringLiteral("/QML_MD3");
            if (hasMd3Headers(nested)) {
                srcRoot = nested;
                break;
            }
        }
    }
    if (!hasMd3Headers(srcRoot)) {
        // md3-create lives beside the library repo
        const QString beside = QDir::cleanPath(
            QCoreApplication::applicationDirPath() + QStringLiteral("/../../QML_MD3"));
        if (hasMd3Headers(beside))
            srcRoot = beside;
        else {
            const QString beside2 = QDir::cleanPath(
                QCoreApplication::applicationDirPath() + QStringLiteral("/../../../QML_MD3"));
            if (hasMd3Headers(beside2))
                srcRoot = beside2;
        }
    }

    QString headerDir = srcRoot + QStringLiteral("/src/Md3");
    if (!QFileInfo::exists(headerDir + QStringLiteral("/md3.h")))
        headerDir = srcRoot;
    if (!QFileInfo::exists(headerDir + QStringLiteral("/md3.h"))
            && !QFileInfo::exists(headerDir + QStringLiteral("/window/md3graphics.h"))) {
        setError(QStringLiteral("找不到头文件 md3.h（源码树）: %1").arg(srcRoot));
        return false;
    }

    const QString libDir = destVendorDir + QStringLiteral("/lib");
    const QString incDir = destVendorDir + QStringLiteral("/include/Md3");
    const QString qmlDir = destVendorDir + QStringLiteral("/qml/Md3");
    const QString stubDir = destVendorDir + QStringLiteral("/stubs");
    QDir().mkpath(libDir);
    QDir().mkpath(incDir);
    QDir().mkpath(qmlDir);
    QDir().mkpath(stubDir);

    // Core static libs
    QString md3LibSrc;
    for (const char *n : {"libMd3.a", "libMd3.lib", "Md3.lib"}) {
        const QString cand = buildMd3 + QLatin1Char('/') + QLatin1String(n);
        if (QFileInfo::exists(cand)) {
            md3LibSrc = cand;
            break;
        }
    }
    if (md3LibSrc.isEmpty()) {
        setError(QStringLiteral("缺少 libMd3"));
        return false;
    }
    if (!copyOneFile(md3LibSrc, libDir + QLatin1Char('/') + QFileInfo(md3LibSrc).fileName())) {
        setError(QStringLiteral("复制 libMd3 失败"));
        return false;
    }

    QString pluginLibSrc = buildMd3 + QStringLiteral("/Md3/libMd3plugin.a");
    if (!QFileInfo::exists(pluginLibSrc))
        pluginLibSrc = buildMd3 + QStringLiteral("/Md3/Md3plugin.lib");
    if (!QFileInfo::exists(pluginLibSrc))
        pluginLibSrc = buildMd3 + QStringLiteral("/libMd3plugin.a");
    if (QFileInfo::exists(pluginLibSrc)) {
        if (!copyOneFile(pluginLibSrc, libDir + QLatin1Char('/') + QFileInfo(pluginLibSrc).fileName())) {
            setError(QStringLiteral("复制 Md3plugin 失败"));
            return false;
        }
    }

    // Headers only (not full QML sources)
    const QStringList headers = {
        QStringLiteral("md3.h"),
        QStringLiteral("window/md3graphics.h"),
        QStringLiteral("window/md3windowhelper.h"),
        QStringLiteral("charts/md3chartdata.h"),
    };
    for (const QString &h : headers) {
        const QString src = headerDir + QLatin1Char('/') + h;
        if (!QFileInfo::exists(src))
            continue;
        const QString dstName = QFileInfo(h).fileName();
        if (!copyOneFile(src, incDir + QLatin1Char('/') + dstName)) {
            setError(QStringLiteral("复制头文件失败: %1").arg(h));
            return false;
        }
    }

    // Module metadata only (qmldir / qmltypes) — not .qml sources
    const QString modDir = buildMd3 + QStringLiteral("/Md3");
    for (const char *meta : {"qmldir", "Md3.qmltypes"}) {
        const QString src = modDir + QLatin1Char('/') + QLatin1String(meta);
        if (QFileInfo::exists(src))
            copyOneFile(src, qmlDir + QLatin1Char('/') + QLatin1String(meta));
    }

    // Tiny static-plugin / rcc init stubs (required for static QML link)
    QStringList stubSrcs;
    const QString pluginInit = buildMd3 + QStringLiteral("/Md3plugin_init.cpp");
    if (QFileInfo::exists(pluginInit))
        stubSrcs << pluginInit;

    QDir rccDir(buildMd3 + QStringLiteral("/.qt/rcc"));
    if (rccDir.exists()) {
        const QStringList inits = rccDir.entryList(QStringList{QStringLiteral("*_init.cpp")},
                                                   QDir::Files);
        for (const QString &f : inits)
            stubSrcs << rccDir.filePath(f);
    }
    // Also gather from CMakeFiles object dirs if only .obj available — prefer .cpp
    for (const QString &s : stubSrcs) {
        if (!copyOneFile(s, stubDir + QLatin1Char('/') + QFileInfo(s).fileName())) {
            setError(QStringLiteral("复制 stub 失败: %1").arg(s));
            return false;
        }
    }

    // Generate imported-target helper
    const QString md3LibName = QFileInfo(md3LibSrc).fileName();
    const QString pluginLibName = QFileInfo::exists(pluginLibSrc)
            ? QFileInfo(pluginLibSrc).fileName() : QString();

    QString cmake;
    cmake += QStringLiteral("# Auto-generated — prebuilt Md3 (no full sources)\n");
    cmake += QStringLiteral("get_filename_component(_MD3_PREBUILT_DIR \"${CMAKE_CURRENT_LIST_DIR}\" ABSOLUTE)\n");
    cmake += QStringLiteral("add_library(Md3 STATIC IMPORTED GLOBAL)\n");
    cmake += QStringLiteral("set_target_properties(Md3 PROPERTIES\n");
    cmake += QStringLiteral("  IMPORTED_LOCATION \"${_MD3_PREBUILT_DIR}/lib/%1\"\n").arg(md3LibName);
    cmake += QStringLiteral("  INTERFACE_INCLUDE_DIRECTORIES \"${_MD3_PREBUILT_DIR}/include/Md3;${_MD3_PREBUILT_DIR}/include\"\n");
    cmake += QStringLiteral("  INTERFACE_LINK_LIBRARIES \"Qt6::Quick;Qt6::QuickControls2;Qt6::QuickEffects;Qt6::QuickShapesPrivate\"\n");
    cmake += QStringLiteral(")\n");
    cmake += QStringLiteral("if (WIN32)\n");
    cmake += QStringLiteral("  set_property(TARGET Md3 APPEND PROPERTY\n");
    cmake += QStringLiteral("    INTERFACE_LINK_LIBRARIES dwmapi shell32 shlwapi ole32 propsys advapi32)\n");
    cmake += QStringLiteral("endif()\n");
    if (!pluginLibName.isEmpty()) {
        cmake += QStringLiteral("add_library(Md3plugin STATIC IMPORTED GLOBAL)\n");
        cmake += QStringLiteral("set_target_properties(Md3plugin PROPERTIES\n");
        cmake += QStringLiteral("  IMPORTED_LOCATION \"${_MD3_PREBUILT_DIR}/lib/%1\"\n").arg(pluginLibName);
        cmake += QStringLiteral(")\n");
    }
    // Compile init stubs into a small static helper
    cmake += QStringLiteral("file(GLOB _MD3_STUBS CONFIGURE_DEPENDS \"${_MD3_PREBUILT_DIR}/stubs/*.cpp\")\n");
    cmake += QStringLiteral("if (_MD3_STUBS)\n");
    cmake += QStringLiteral("  add_library(Md3plugin_init STATIC ${_MD3_STUBS})\n");
    cmake += QStringLiteral("  target_link_libraries(Md3plugin_init PUBLIC Qt6::Qml Qt6::Quick)\n");
    cmake += QStringLiteral("endif()\n");
    cmake += QStringLiteral("list(APPEND CMAKE_QML_IMPORT_PATH \"${_MD3_PREBUILT_DIR}/qml\")\n");
    cmake += QStringLiteral("set(CMAKE_QML_IMPORT_PATH \"${CMAKE_QML_IMPORT_PATH}\" CACHE STRING \"\" FORCE)\n");

    QFile out(destVendorDir + QStringLiteral("/Md3Prebuilt.cmake"));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        setError(QStringLiteral("无法写入 Md3Prebuilt.cmake"));
        return false;
    }
    out.write(cmake.toUtf8());
    return true;
}

bool ProjectGenerator::generate(const QVariantMap &options)
{
    setBusy(true);
    m_lastError.clear();
    emit lastErrorChanged();

    const QString requestedName = options.value(QStringLiteral("name")).toString().trimmed();
    const QString outputParent = options.value(QStringLiteral("outputDir")).toString().trimmed();
    QString uri = options.value(QStringLiteral("uri")).toString().trimmed();
    const QString tmpl = options.value(QStringLiteral("template")).toString().trimmed();
    const bool dark = options.value(QStringLiteral("dark")).toBool();
    const QString seed = options.value(QStringLiteral("seed"), QStringLiteral("#6750A4")).toString();
    const bool force = options.value(QStringLiteral("force")).toBool();
    const bool md3Absolute = options.value(QStringLiteral("md3Absolute")).toBool();
    const bool copyLibrary = options.value(QStringLiteral("copyLibrary")).toBool();
    const QString vendorFolder = options.value(QStringLiteral("vendorFolder"),
                                               QStringLiteral("vendor/Md3")).toString().trimmed();

    // Auto-dedupe project folder name unless force overwrite
    QString name = requestedName;
    if (!force)
        name = suggestProjectName(outputParent, requestedName.isEmpty() ? QStringLiteral("MyMd3App")
                                                                        : requestedName);
    else if (name.isEmpty())
        name = QStringLiteral("MyMd3App");

    // Keep URI in sync when user left it empty or matching the typed name
    if (uri.isEmpty() || uri == requestedName)
        uri = name;

    // Selected kits: list of maps {version, kit, prefix, label?}
    QVariantList selectedKits = options.value(QStringLiteral("kits")).toList();
    if (selectedKits.isEmpty()) {
        // Back-compat single kit fields
        const QString qtVersion = options.value(QStringLiteral("qtVersion")).toString().trimmed();
        const QString qtKit = options.value(QStringLiteral("qtKit")).toString().trimmed();
        const QString qtPrefix = options.value(QStringLiteral("qtPrefix")).toString().trimmed();
        if (!qtPrefix.isEmpty()) {
            QVariantMap one;
            one.insert(QStringLiteral("version"), qtVersion);
            one.insert(QStringLiteral("kit"), qtKit);
            one.insert(QStringLiteral("prefix"), qtPrefix);
            one.insert(QStringLiteral("label"),
                       QStringLiteral("Qt %1 / %2").arg(qtVersion, qtKit));
            selectedKits.append(one);
        }
    }

    QString md3Override = options.value(QStringLiteral("md3Path")).toString().trimmed();
    if (md3Override.isEmpty())
        md3Override = m_md3Path;
    md3Override = normalizeMd3Root(md3Override);

    static const QRegularExpression nameRe(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9_]*$)"));
    if (!nameRe.match(name).hasMatch()) {
        setError(QStringLiteral("项目名须为标识符（字母开头）"));
        setBusy(false);
        return false;
    }
    if (!QRegularExpression(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9_.]*$)")).match(uri).hasMatch()) {
        setError(QStringLiteral("QML URI 无效"));
        setBusy(false);
        return false;
    }
    if (tmpl != QLatin1String("empty") && tmpl != QLatin1String("basic") && tmpl != QLatin1String("rail")) {
        setError(QStringLiteral("模板须为 empty / basic / rail"));
        setBusy(false);
        return false;
    }
    if (copyLibrary) {
        const QString probe = options.value(QStringLiteral("md3Path")).toString().trimmed();
        const QString look = probe.isEmpty() ? md3Override : probe;
        if (findPrebuiltMd3Dir(look).isEmpty()) {
            setError(QStringLiteral(
                "未找到预编译 libMd3。请先编译组件库，或把路径指到含 libMd3.a/.lib 的构建目录"
                "（如 …/build-mingw/md3/src/Md3）。"));
            setBusy(false);
            return false;
        }
    } else if (!isValidMd3Path(md3Override)) {
        setError(QStringLiteral("Md3 库目录无效: %1").arg(md3Override));
        setBusy(false);
        return false;
    }
    if (selectedKits.isEmpty()) {
        setError(QStringLiteral("请至少选择一个编译器 / Kit"));
        setBusy(false);
        return false;
    }

    const QString dest = QDir(outputParent).filePath(name);
    QDir destDir(dest);
    if (destDir.exists()) {
        const QStringList entries = destDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (!entries.isEmpty() && !force) {
            setError(QStringLiteral("目标已存在且非空: %1").arg(dest));
            setBusy(false);
            return false;
        }
    } else if (!QDir().mkpath(dest)) {
        setError(QStringLiteral("无法创建目录: %1").arg(dest));
        setBusy(false);
        return false;
    }

    QString md3CmakeBlock;
    QString md3ForCmake;
    QString absMd3;

    if (copyLibrary) {
        QString vendorRel = vendorFolder.isEmpty() ? QStringLiteral("vendor/Md3") : vendorFolder;
        vendorRel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (vendorRel.startsWith(QLatin1Char('/')))
            vendorRel.remove(0, 1);
        const QString vendorAbs = destDir.filePath(vendorRel);
        const QString probe = options.value(QStringLiteral("md3Path")).toString().trimmed();
        const QString copyFrom = probe.isEmpty() ? md3Override : probe;
        if (!copyPrebuiltMd3(copyFrom, vendorAbs)) {
            setBusy(false);
            return false;
        }
        md3ForCmake = vendorRel;
        absMd3 = cmakePath(vendorAbs);
        md3CmakeBlock =
            QStringLiteral("# Prebuilt Md3 (libs + headers only)\n"
                           "include(\"${CMAKE_CURRENT_SOURCE_DIR}/%1/Md3Prebuilt.cmake\")\n")
                .arg(vendorRel);
    } else {
        absMd3 = cmakePath(md3Override);
        if (md3Absolute) {
            md3ForCmake = absMd3;
        } else {
            const QString absDest = cmakePath(dest);
            const QString rel = QDir(absDest).relativeFilePath(absMd3);
            md3ForCmake = rel.isEmpty() ? absMd3 : rel;
        }
        md3CmakeBlock =
            QStringLiteral(
                "# Md3 sources via add_subdirectory — override with -DMD3_ROOT=...\n"
                "set(MD3_ROOT \"%1\" CACHE PATH \"Path to QML_MD3 repository\")\n"
                "if (NOT EXISTS \"${MD3_ROOT}/CMakeLists.txt\")\n"
                "    message(FATAL_ERROR \"MD3_ROOT invalid (no CMakeLists.txt): ${MD3_ROOT}\")\n"
                "endif()\n"
                "set(MD3_BUILD_GALLERY OFF CACHE BOOL \"Skip Gallery demo\" FORCE)\n"
                "add_subdirectory(\"${MD3_ROOT}\" \"${CMAKE_CURRENT_BINARY_DIR}/md3\")\n")
                .arg(md3ForCmake);
    }

    QString target = name.toLower();
    target.replace(QRegularExpression(QStringLiteral("[^a-z0-9_]")), QStringLiteral("_"));
    if (!target.isEmpty() && target[0].isDigit())
        target.prepend(QStringLiteral("app_"));

    // Use first kit for find_package version floor
    const QVariantMap firstKit = selectedKits.first().toMap();
    const QString qtVersion = firstKit.value(QStringLiteral("version")).toString();
    const QString qtKit = firstKit.value(QStringLiteral("kit")).toString();
    const QString qtPrefix = firstKit.value(QStringLiteral("prefix")).toString();
    const QStringList verParts = qtVersion.split(QLatin1Char('.'));
    QString qtMm = QStringLiteral("6.8");
    if (verParts.size() >= 2 && verParts[0].at(0).isDigit())
        qtMm = verParts[0] + QLatin1Char('.') + verParts[1];

    QString extraQml;
    if (tmpl == QLatin1String("rail")) {
        extraQml = QStringLiteral(
            "        pages/HomePage.qml\n"
            "        pages/WidgetsPage.qml\n"
            "        pages/SettingsPage.qml");
    }

    QMap<QString, QString> vars;
    vars.insert(QStringLiteral("PROJECT_NAME"), name);
    vars.insert(QStringLiteral("TARGET_NAME"), target);
    vars.insert(QStringLiteral("QML_URI"), uri);
    vars.insert(QStringLiteral("QT_VERSION"), qtVersion.isEmpty() ? QStringLiteral("6.8") : qtVersion);
    vars.insert(QStringLiteral("QT_VERSION_MM"), qtMm);
    vars.insert(QStringLiteral("QT_KIT"), qtKit);
    vars.insert(QStringLiteral("QT_PREFIX"), cmakePath(qtPrefix));
    vars.insert(QStringLiteral("MD3_PATH"), md3ForCmake);
    vars.insert(QStringLiteral("MD3_PATH_ABS"), absMd3);
    vars.insert(QStringLiteral("MD3_CMAKE_BLOCK"), md3CmakeBlock);
    vars.insert(QStringLiteral("TEMPLATE"), tmpl);
    vars.insert(QStringLiteral("DARK_DEFAULT"), dark ? QStringLiteral("true") : QStringLiteral("false"));
    vars.insert(QStringLiteral("SEED_COLOR"), seed);
    vars.insert(QStringLiteral("APP_TITLE"), name);
    vars.insert(QStringLiteral("EXTRA_QML_FILES"), extraQml);

    const QString base = QStringLiteral(":/md3-create/templates");
    auto ok = [&](const QString &qrc, const QString &rel) {
        return writeRendered(base + qrc, destDir.filePath(rel), vars);
    };

    if (!ok(QStringLiteral("/_common/CMakeLists.txt.in"), QStringLiteral("CMakeLists.txt")))
        { setBusy(false); return false; }
    if (!ok(QStringLiteral("/_common/main.cpp.in"), QStringLiteral("main.cpp")))
        { setBusy(false); return false; }
    if (!ok(QStringLiteral("/_common/README.md.in"), QStringLiteral("README.md")))
        { setBusy(false); return false; }
    if (!writeBytes(base + QStringLiteral("/_common/.gitignore"), destDir.filePath(QStringLiteral(".gitignore"))))
        { setBusy(false); return false; }

    if (tmpl == QLatin1String("empty")) {
        if (!ok(QStringLiteral("/empty/Main.qml.in"), QStringLiteral("Main.qml")))
            { setBusy(false); return false; }
    } else if (tmpl == QLatin1String("basic")) {
        if (!ok(QStringLiteral("/basic/Main.qml.in"), QStringLiteral("Main.qml")))
            { setBusy(false); return false; }
    } else {
        if (!ok(QStringLiteral("/rail/Main.qml.in"), QStringLiteral("Main.qml")))
            { setBusy(false); return false; }
        if (!ok(QStringLiteral("/rail/pages/HomePage.qml.in"), QStringLiteral("pages/HomePage.qml")))
            { setBusy(false); return false; }
        if (!ok(QStringLiteral("/rail/pages/WidgetsPage.qml.in"), QStringLiteral("pages/WidgetsPage.qml")))
            { setBusy(false); return false; }
        if (!ok(QStringLiteral("/rail/pages/SettingsPage.qml.in"), QStringLiteral("pages/SettingsPage.qml")))
            { setBusy(false); return false; }
    }

    // CMakePresets.json — one configure/build preset per selected kit
    QJsonArray configurePresets;
    QJsonArray buildPresets;
    QString firstPresetName;
    for (int i = 0; i < selectedKits.size(); ++i) {
        const QVariantMap kit = selectedKits.at(i).toMap();
        const QString ver = kit.value(QStringLiteral("version")).toString();
        const QString kn = kit.value(QStringLiteral("kit")).toString();
        const QString prefix = cmakePath(kit.value(QStringLiteral("prefix")).toString());
        if (prefix.isEmpty() || !QFileInfo::exists(prefix))
            continue;

        QString presetName = QStringLiteral("%1-%2").arg(ver, kn);
        presetName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.+-]")), QStringLiteral("_"));
        if (presetName.isEmpty())
            presetName = QStringLiteral("kit%1").arg(i);
        if (firstPresetName.isEmpty())
            firstPresetName = presetName;

        const QString binDir = selectedKits.size() == 1
            ? QStringLiteral("${sourceDir}/build")
            : QStringLiteral("${sourceDir}/build/%1").arg(presetName);

        QJsonObject cache{
            {QStringLiteral("CMAKE_BUILD_TYPE"), QStringLiteral("Debug")},
            {QStringLiteral("CMAKE_PREFIX_PATH"), prefix},
            {QStringLiteral("CMAKE_CXX_STANDARD"), QStringLiteral("17")},
        };
        // External source tree only — prebuilt vendor uses Md3Prebuilt.cmake
        if (!copyLibrary)
            cache.insert(QStringLiteral("MD3_ROOT"), absMd3);

        configurePresets.append(QJsonObject{
            {QStringLiteral("name"), presetName},
            {QStringLiteral("displayName"),
             kit.value(QStringLiteral("label")).toString().isEmpty()
                 ? QStringLiteral("Qt %1 / %2").arg(ver, kn)
                 : kit.value(QStringLiteral("label")).toString()},
            {QStringLiteral("generator"), QStringLiteral("Ninja")},
            {QStringLiteral("binaryDir"), binDir},
            {QStringLiteral("cacheVariables"), cache},
        });
        buildPresets.append(QJsonObject{
            {QStringLiteral("name"), presetName},
            {QStringLiteral("configurePreset"), presetName},
        });
    }

    // Alias "default" -> first kit for convenience
    if (!firstPresetName.isEmpty() && firstPresetName != QLatin1String("default")) {
        configurePresets.prepend(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("default")},
            {QStringLiteral("displayName"), QStringLiteral("Default (%1)").arg(firstPresetName)},
            {QStringLiteral("inherits"), QJsonArray{firstPresetName}},
        });
        buildPresets.prepend(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("default")},
            {QStringLiteral("configurePreset"), QStringLiteral("default")},
        });
    }

    if (!configurePresets.isEmpty()) {
        QJsonObject root{
            {QStringLiteral("version"), 6},
            {QStringLiteral("cmakeMinimumRequired"), QJsonObject{
                {QStringLiteral("major"), 3},
                {QStringLiteral("minor"), 21},
                {QStringLiteral("patch"), 0},
            }},
            {QStringLiteral("configurePresets"), configurePresets},
            {QStringLiteral("buildPresets"), buildPresets},
        };
        QFile presets(destDir.filePath(QStringLiteral("CMakePresets.json")));
        if (presets.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            presets.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    m_lastOutput = dest;
    emit lastOutputChanged();
    emit generated(dest);
    setBusy(false);
    return true;
}
