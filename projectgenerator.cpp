#include "projectgenerator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

bool hasQt6ConfigUnderLib(const QString &libDir)
{
    if (QFileInfo::exists(libDir + QStringLiteral("/cmake/Qt6/Qt6Config.cmake")))
        return true;
    const QDir d(libDir);
    if (!d.exists())
        return false;
    const QStringList subs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &sub : subs) {
        if (QFileInfo::exists(d.filePath(sub) + QStringLiteral("/cmake/Qt6/Qt6Config.cmake")))
            return true;
    }
    return false;
}

} // namespace

bool ProjectGenerator::isQt6Prefix(const QString &prefix)
{
    const QString p = QDir::cleanPath(prefix);
    if (p.isEmpty() || !QFileInfo(p).isDir())
        return false;
    if (hasQt6ConfigUnderLib(p + QStringLiteral("/lib")))
        return true;
    // Debian/Ubuntu multiarch sometimes under /usr/lib/<triplet> with prefix /usr
    if (hasQt6ConfigUnderLib(p + QStringLiteral("/lib64")))
        return true;
    const QStringList qmakes = {
        p + QStringLiteral("/bin/qmake6"),
        p + QStringLiteral("/bin/qmake"),
#if defined(Q_OS_WIN)
        p + QStringLiteral("/bin/qmake.exe"),
#endif
    };
    for (const QString &qm : qmakes) {
        if (QFileInfo::exists(qm))
            return true;
    }
    return false;
}

QString ProjectGenerator::queryQtInstallPrefix()
{
    const QStringList tools = {
        QStringLiteral("qtpaths6"),
        QStringLiteral("qtpaths"),
        QStringLiteral("qmake6"),
        QStringLiteral("qmake"),
    };
    for (const QString &tool : tools) {
        const QString exe = QStandardPaths::findExecutable(tool);
        if (exe.isEmpty())
            continue;
        QProcess proc;
        proc.setProgram(exe);
        if (tool.startsWith(QLatin1String("qtpaths")))
            proc.setArguments({QStringLiteral("--install-prefix")});
        else
            proc.setArguments({QStringLiteral("-query"), QStringLiteral("QT_INSTALL_PREFIX")});
        proc.start();
        if (!proc.waitForFinished(3000) || proc.exitCode() != 0)
            continue;
        const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
        if (!out.isEmpty() && QFileInfo(out).isDir())
            return QDir::cleanPath(out);
    }
    return {};
}

QString ProjectGenerator::detectDefaultQtRoot()
{
    const QByteArray envRoot = qgetenv("QT_ROOT");
    if (!envRoot.isEmpty()) {
        const QString p = QDir::cleanPath(QString::fromLocal8Bit(envRoot));
        if (QFileInfo(p).isDir())
            return p;
    }

    // Prefer the Qt prefix used to build / run this app (MSVC kit, system /usr, …).
    const QString fromPath = queryQtInstallPrefix();
    if (!fromPath.isEmpty() && QFileInfo(fromPath).isDir())
        return QDir::cleanPath(fromPath);

    const QByteArray cpp = qgetenv("CMAKE_PREFIX_PATH");
    if (!cpp.isEmpty()) {
        for (const QString &part :
             QString::fromLocal8Bit(cpp).split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
            const QString p = QDir::cleanPath(part.trimmed());
            if (!p.isEmpty() && isQt6Prefix(p))
                return p;
        }
    }

    // Online installer roots
    const QString homeQt = QDir::cleanPath(QDir::homePath() + QStringLiteral("/Qt"));
#if defined(Q_OS_WIN)
    const QStringList candidates = {
        QStringLiteral("D:/Qt"),
        QStringLiteral("C:/Qt"),
        homeQt,
        QDir::cleanPath(QDir::homePath() + QStringLiteral("/AppData/Local/Qt")),
    };
#else
    const QStringList candidates = {
        homeQt,
        QStringLiteral("/opt/Qt"),
        QStringLiteral("/usr"),
    };
#endif
    for (const QString &c : candidates) {
        if (QFileInfo(c).isDir())
            return c;
    }

#if defined(Q_OS_WIN)
    return QStringLiteral("D:/Qt");
#else
    return homeQt;
#endif
}

bool ProjectGenerator::tryAddKitPrefix(const QString &prefixPath, bool custom)
{
    const QString prefix = cmakePath(prefixPath);
    if (!isQt6Prefix(prefix))
        return false;

    for (const QVariant &v : m_kits) {
        if (v.toMap().value(QStringLiteral("prefix")).toString() == prefix)
            return true;
    }

    QDir kitDir(prefix);
    const QString kitName = kitDir.dirName();
    QString ver = QStringLiteral("custom");
    if (kitDir.cdUp()) {
        const QString maybeVer = kitDir.dirName();
        static const QRegularExpression verRe(QStringLiteral(R"(^\d+\.\d+(\.\d+)?$)"));
        if (verRe.match(maybeVer).hasMatch())
            ver = maybeVer;
        else if (kitName == QLatin1String("usr") || prefix == QLatin1String("/usr"))
            ver = QStringLiteral("system");
        else
            ver = maybeVer;
    }

    QVariantMap row;
    row.insert(QStringLiteral("label"), QStringLiteral("Qt %1 / %2").arg(ver, kitName));
    row.insert(QStringLiteral("version"), ver);
    row.insert(QStringLiteral("kit"), kitName);
    row.insert(QStringLiteral("prefix"), prefix);
    if (custom)
        row.insert(QStringLiteral("custom"), true);
    m_kits.append(row);
    return true;
}

void ProjectGenerator::scanInstallerKits(const QString &rootPath)
{
    QDir root(rootPath);
    if (!root.exists())
        return;

    const QStringList versions = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    static const QRegularExpression verRe(QStringLiteral(R"(^\d+\.\d+(\.\d+)?$)"));
    for (const QString &ver : versions) {
        if (!verRe.match(ver).hasMatch())
            continue;
        QDir verDir(root.filePath(ver));
        const QStringList kits = verDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &kit : kits)
            tryAddKitPrefix(verDir.filePath(kit), false);
    }
}

ProjectGenerator::ProjectGenerator(QObject *parent)
    : QObject(parent)
{
    m_qtRoot = detectDefaultQtRoot();
    // Fixed layout: Md3 package sits next to Md3Create.exe (same directory).
    // Fallbacks: ../Md3, sibling QML_MD3 sources, local build trees.
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList guesses = {
        QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("Md3"))),
        QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("../Md3"))),
        QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("../../dist/Md3"))),
        QDir::cleanPath(QFileInfo(QStringLiteral(__FILE__)).absoluteDir()
                            .absoluteFilePath(QStringLiteral("../QML_MD3/dist/Md3"))),
        QDir::cleanPath(QFileInfo(QStringLiteral(__FILE__)).absoluteDir()
                            .absoluteFilePath(QStringLiteral("../QML_MD3"))),
        QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("../../QML_MD3"))),
    };
    QString guess;
    for (const QString &g : guesses) {
        if (isValidMd3Path(g)) {
            guess = g;
            break;
        }
    }
    if (guess.isEmpty())
        guess = guesses.first(); // still point at same-dir Md3 for the UI
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
    // Packaged install tree (scripts/package-*.sh|ps1 → dist/Md3)
    if (isPackagedMd3Dir(p))
        return true;
    // Prebuilt build tree (contains libMd3)
    if (!findPrebuiltMd3Dir(p).isEmpty())
        return true;
    return QFileInfo::exists(p + QStringLiteral("/../CMakeLists.txt"))
            && QFileInfo(p).fileName() == QLatin1String("Md3");
}

bool ProjectGenerator::isPackagedMd3Dir(const QString &path) const
{
    const QString p = QDir::cleanPath(path);
    const bool hasLib =
            QFileInfo::exists(p + QStringLiteral("/lib/libMd3.a"))
            || QFileInfo::exists(p + QStringLiteral("/lib/libMd3.lib"))
            || QFileInfo::exists(p + QStringLiteral("/lib/Md3.lib"))
            || QFileInfo::exists(p + QStringLiteral("/lib/libMd3.so"))
            || QFileInfo::exists(p + QStringLiteral("/lib/libMd3.dylib"))
            || !QDir(p + QStringLiteral("/lib")).entryList({QStringLiteral("libMd3.so*")},
                                                          QDir::Files).isEmpty();
    const bool hasHeaders =
            QFileInfo::exists(p + QStringLiteral("/include/Md3/md3.h"))
            || QFileInfo::exists(p + QStringLiteral("/include/md3.h"));
    const bool hasCmake =
            QFileInfo::exists(p + QStringLiteral("/lib/cmake/Md3/Md3Config.cmake"))
            || QFileInfo::exists(p + QStringLiteral("/Md3Prebuilt.cmake"));
    return hasLib && (hasHeaders || hasCmake);
}

QString ProjectGenerator::findPrebuiltMd3Dir(const QString &path) const
{
    const QString p = QDir::cleanPath(path);
    auto hasLib = [](const QString &dir) {
        return QFileInfo::exists(dir + QStringLiteral("/libMd3.a"))
                || QFileInfo::exists(dir + QStringLiteral("/libMd3.lib"))
                || QFileInfo::exists(dir + QStringLiteral("/Md3.lib"))
                || QFileInfo::exists(dir + QStringLiteral("/libMd3.so"));
    };

    QStringList candidates;
    // Packaged layout: PREFIX/lib/libMd3.*
    if (isPackagedMd3Dir(p))
        return p + QStringLiteral("/lib");

    candidates << p
               << p + QStringLiteral("/lib")
               << p + QStringLiteral("/src/Md3")
               << p + QStringLiteral("/build-lib/src/Md3")
               << p + QStringLiteral("/build/src/Md3")
               << p + QStringLiteral("/build-mingw/src/Md3")
               << p + QStringLiteral("/build-mingw/md3/src/Md3");

    // Sibling md3-create build (common local layout)
    const QString siblingCreate = QDir(p + QStringLiteral("/../md3-create")).absolutePath();
    candidates << siblingCreate + QStringLiteral("/build-mingw/md3/src/Md3")
               << siblingCreate + QStringLiteral("/build/md3/src/Md3");

    // Same-dir package next to Create
    const QString appMd3 = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/Md3"));
    if (isPackagedMd3Dir(appMd3))
        candidates.prepend(appMd3 + QStringLiteral("/lib"));

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

static int detectPackagedMd3SharedFlag(const QString &md3PackageDir)
{
    const QString cfg = md3PackageDir + QStringLiteral("/lib/cmake/Md3/Md3Config.cmake");
    QFile f(cfg);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString text = QString::fromUtf8(f.readAll());
        const QRegularExpression re(QStringLiteral(R"(set\(_MD3_PACKAGE_SHARED\s+([01])\))"));
        const QRegularExpressionMatch m = re.match(text);
        if (m.hasMatch())
            return m.captured(1) == QLatin1String("1") ? 1 : 0;
    }

    // Fallbacks for copied/prebuilt layouts
    if (QFileInfo::exists(md3PackageDir + QStringLiteral("/bin/Md3.dll"))
            || QFileInfo::exists(md3PackageDir + QStringLiteral("/bin/libMd3.so"))
            || QFileInfo::exists(md3PackageDir + QStringLiteral("/bin/libMd3.dylib")))
        return 1;
    if (QFileInfo::exists(md3PackageDir + QStringLiteral("/Md3Prebuilt.cmake")))
        return 0;
    return -1; // unknown
}

bool ProjectGenerator::addCustomKit(const QString &prefixPath)
{
    const QString prefix = cmakePath(prefixPath);
    if (prefix.isEmpty() || !QFileInfo::exists(prefix)) {
        setError(QStringLiteral("Kit 路径无效"));
        return false;
    }
    if (!isQt6Prefix(prefix)) {
        setError(QStringLiteral("不是有效的 Qt Kit 前缀: %1").arg(prefix));
        return false;
    }
    if (!tryAddKitPrefix(prefix, true)) {
        setError(QStringLiteral("添加 Kit 失败: %1").arg(prefix));
        return false;
    }
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
    // Keep user-added custom kits across rescans of the Qt root.
    QVariantList preserved;
    for (const QVariant &v : std::as_const(m_kits)) {
        if (v.toMap().value(QStringLiteral("custom")).toBool())
            preserved.append(v);
    }

    m_kits.clear();

    // Only scan the Qt root shown in the UI — avoids silently mixing in MinGW from
    // other install paths (D:/Qt vs C:/Qt, CMAKE_PREFIX_PATH, PATH, …).
    if (!m_qtRoot.trimmed().isEmpty()) {
        scanInstallerKits(m_qtRoot);
        if (isQt6Prefix(m_qtRoot))
            tryAddKitPrefix(m_qtRoot, false);
    }

    for (const QVariant &v : std::as_const(preserved)) {
        const QString prefix = v.toMap().value(QStringLiteral("prefix")).toString();
        if (!prefix.isEmpty())
            tryAddKitPrefix(prefix, true);
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

bool ProjectGenerator::copyDirectoryRecursively(const QString &src, const QString &dst)
{
    QDir srcDir(src);
    if (!srcDir.exists())
        return false;
    QDir().mkpath(dst);
    const QFileInfoList entries = srcDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString to = dst + QLatin1Char('/') + fi.fileName();
        if (fi.isDir()) {
            if (!copyDirectoryRecursively(fi.absoluteFilePath(), to))
                return false;
        } else if (!copyOneFile(fi.absoluteFilePath(), to)) {
            return false;
        }
    }
    return true;
}

bool ProjectGenerator::copyPackagedMd3Tree(const QString &packageDir, const QString &destDir)
{
    if (QDir(destDir).exists())
        QDir(destDir).removeRecursively();
    if (!copyDirectoryRecursively(packageDir, destDir)) {
        setError(QStringLiteral("复制 Md3 包失败: %1 → %2").arg(packageDir, destDir));
        return false;
    }
    // Ensure a drop-in helper exists for apps that prefer include() over find_package
    const QString prebuilt = destDir + QStringLiteral("/Md3Prebuilt.cmake");
    if (!QFileInfo::exists(prebuilt)
            && QFileInfo::exists(destDir + QStringLiteral("/lib/cmake/Md3/Md3Config.cmake"))) {
        QFile out(prebuilt);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            out.write(
                "# Wrapper — prefer find_package via CMAKE_PREFIX_PATH\n"
                "list(APPEND CMAKE_PREFIX_PATH \"${CMAKE_CURRENT_LIST_DIR}\")\n"
                "find_package(Md3 REQUIRED CONFIG)\n");
        }
    }
    return true;
}

bool ProjectGenerator::copyPrebuiltMd3(const QString &sourceOrBuildPath, const QString &destVendorDir)
{
    // Already a packaged tree (same-dir Md3 next to Create, or dist/Md3)
    if (isPackagedMd3Dir(sourceOrBuildPath))
        return copyPackagedMd3Tree(sourceOrBuildPath, destVendorDir);

    const QString buildMd3 = findPrebuiltMd3Dir(sourceOrBuildPath);
    if (buildMd3.isEmpty()) {
        setError(QStringLiteral(
            "未找到预编译 libMd3（.a/.lib）。请先运行 scripts/package-windows.ps1（或 package-linux.sh），"
            "并把 Md3 包放在 Md3Create.exe 同目录下（文件夹名固定为 Md3）。"));
        return false;
    }

    // If findPrebuilt pointed at package lib/, copy parent package
    const QString maybePkg = QDir::cleanPath(buildMd3 + QStringLiteral("/.."));
    if (isPackagedMd3Dir(maybePkg))
        return copyPackagedMd3Tree(maybePkg, destVendorDir);

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
            const QString nested = abs + QStringLiteral("/QML_MD3");
            if (hasMd3Headers(nested)) {
                srcRoot = nested;
                break;
            }
        }
    }
    if (!hasMd3Headers(srcRoot)) {
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
    for (const char *n : {"libMd3.a", "libMd3.lib", "Md3.lib", "libMd3.so"}) {
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
    if (!QFileInfo::exists(pluginLibSrc))
        pluginLibSrc = buildMd3 + QStringLiteral("/libMd3plugin.lib");
    if (QFileInfo::exists(pluginLibSrc)) {
        if (!copyOneFile(pluginLibSrc, libDir + QLatin1Char('/') + QFileInfo(pluginLibSrc).fileName())) {
            setError(QStringLiteral("复制 Md3plugin 失败"));
            return false;
        }
    }

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

    const QString modDir = buildMd3 + QStringLiteral("/Md3");
    for (const char *meta : {"qmldir", "Md3.qmltypes"}) {
        const QString src = modDir + QLatin1Char('/') + QLatin1String(meta);
        if (QFileInfo::exists(src))
            copyOneFile(src, qmlDir + QLatin1Char('/') + QLatin1String(meta));
    }

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
    for (const QString &s : stubSrcs) {
        if (!copyOneFile(s, stubDir + QLatin1Char('/') + QFileInfo(s).fileName())) {
            setError(QStringLiteral("复制 stub 失败: %1").arg(s));
            return false;
        }
    }

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
    const QString md3Linkage = options.value(QStringLiteral("md3Linkage"),
                                             QStringLiteral("auto")).toString().trimmed().toLower();
    const QString buildType = options.value(QStringLiteral("buildType"),
                                            QStringLiteral("Release")).toString().trimmed();
    const QString vendorFolder = options.value(QStringLiteral("vendorFolder"),
                                               QStringLiteral("Md3")).toString().trimmed();

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
    // Generated apps always embed ./Md3 — require a packaged or prebuilt source
    {
        const QString probe = options.value(QStringLiteral("md3Path")).toString().trimmed();
        const QString look = probe.isEmpty() ? md3Override : probe;
        const QString besideCreate = QDir::cleanPath(
            QCoreApplication::applicationDirPath() + QStringLiteral("/Md3"));
        if (findPrebuiltMd3Dir(look).isEmpty()
                && !isPackagedMd3Dir(look)
                && !isPackagedMd3Dir(besideCreate)) {
            setError(QStringLiteral(
                "未找到预编译 Md3。请先运行 scripts/package-windows.ps1（或 package-linux.sh），"
                "并将包放到 Md3Create 同目录下的 Md3/ 文件夹。"));
            setBusy(false);
            return false;
        }
    }
    Q_UNUSED(copyLibrary);
    Q_UNUSED(md3Absolute);
    Q_UNUSED(vendorFolder);
    if (md3Linkage != QLatin1String("auto")
            && md3Linkage != QLatin1String("shared")
            && md3Linkage != QLatin1String("static")) {
        setError(QStringLiteral("Md3 链接方式无效（应为 auto/shared/static）"));
        setBusy(false);
        return false;
    }
    if (buildType != QLatin1String("Debug")
            && buildType != QLatin1String("Release")
            && buildType != QLatin1String("RelWithDebInfo")
            && buildType != QLatin1String("MinSizeRel")) {
        setError(QStringLiteral("构建类型无效（Debug/Release/RelWithDebInfo/MinSizeRel）"));
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

    // Always vendor packaged Md3 as ./Md3 (same directory as CMakeLists.txt).
    // Template CMakeLists.txt hardcodes CMAKE_CURRENT_SOURCE_DIR/Md3.
    QString md3ForCmake = QStringLiteral("Md3");
    QString absMd3;
    {
        const QString vendorRel = QStringLiteral("Md3");
        const QString vendorAbs = destDir.filePath(vendorRel);
        const QString probe = options.value(QStringLiteral("md3Path")).toString().trimmed();
        QString copyFrom = probe.isEmpty() ? md3Override : probe;
        // Prefer Create.exe sibling ./Md3 when available
        const QString besideCreate = QDir::cleanPath(
            QCoreApplication::applicationDirPath() + QStringLiteral("/Md3"));
        if (isPackagedMd3Dir(besideCreate))
            copyFrom = besideCreate;
        if (!copyPrebuiltMd3(copyFrom, vendorAbs)) {
            setBusy(false);
            return false;
        }
        absMd3 = cmakePath(vendorAbs);

        if (md3Linkage != QLatin1String("auto")) {
            const int sharedFlag = detectPackagedMd3SharedFlag(vendorAbs);
            if (sharedFlag < 0) {
                setError(QStringLiteral(
                    "无法识别 Md3 包是动态还是静态。请使用 scripts/package-windows.ps1 或 package-linux.sh 重新打包。"));
                setBusy(false);
                return false;
            }
            const bool expectShared = (md3Linkage == QLatin1String("shared"));
            const bool actualShared = (sharedFlag == 1);
            if (expectShared != actualShared) {
                setError(QStringLiteral(
                    "Md3 包类型不匹配：你选择了%1，但当前包是%2。请重打包后再创建。\n"
                    "Windows: scripts/package-windows.ps1 -Shared:%3\n"
                    "Linux:   SHARED=%4 ./scripts/package-linux.sh")
                    .arg(expectShared ? QStringLiteral("动态库") : QStringLiteral("静态库"))
                    .arg(actualShared ? QStringLiteral("动态库") : QStringLiteral("静态库"))
                    .arg(expectShared ? QStringLiteral("$true") : QStringLiteral("$false"))
                    .arg(expectShared ? QStringLiteral("1") : QStringLiteral("0")));
                setBusy(false);
                return false;
            }
        }
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
    vars.insert(QStringLiteral("TEMPLATE"), tmpl);
    vars.insert(QStringLiteral("DARK_DEFAULT"), dark ? QStringLiteral("true") : QStringLiteral("false"));
    vars.insert(QStringLiteral("SEED_COLOR"), seed);
    vars.insert(QStringLiteral("APP_TITLE"), name);
    vars.insert(QStringLiteral("EXTRA_QML_FILES"), extraQml);
    vars.insert(QStringLiteral("CMAKE_BUILD_TYPE"), buildType);

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

    QFile::remove(destDir.filePath(QStringLiteral("CMakePresets.json")));

    m_lastOutput = dest;
    emit lastOutputChanged();
    emit generated(dest);
    setBusy(false);
    return true;
}
