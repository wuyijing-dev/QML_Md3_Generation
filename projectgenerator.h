#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

/// Scans Qt installs and writes Md3 QML app projects from embedded templates.
class ProjectGenerator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList kits READ kits NOTIFY kitsChanged)
    Q_PROPERTY(QString qtRoot READ qtRoot WRITE setQtRoot NOTIFY qtRootChanged)
    Q_PROPERTY(QString md3Path READ md3Path WRITE setMd3Path NOTIFY md3PathChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastOutput READ lastOutput NOTIFY lastOutputChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit ProjectGenerator(QObject *parent = nullptr);

    QVariantList kits() const { return m_kits; }
    QString qtRoot() const { return m_qtRoot; }
    void setQtRoot(const QString &path);
    QString md3Path() const { return m_md3Path; }
    void setMd3Path(const QString &path);
    QString lastError() const { return m_lastError; }
    QString lastOutput() const { return m_lastOutput; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void refreshKits();
    Q_INVOKABLE QString pickDirectory(const QString &title, const QString &startDir = QString());
    /// Append a custom kit from an arbitrary Qt prefix directory.
    Q_INVOKABLE bool addCustomKit(const QString &prefixPath);
    Q_INVOKABLE bool generate(const QVariantMap &options);

    /// Validate that path looks like a QML_MD3 / Md3 source tree.
    Q_INVOKABLE bool isValidMd3Path(const QString &path) const;

    /// Return a unique project folder name under outputDir (MyApp, MyApp2, …).
    Q_INVOKABLE QString uniqueProjectName(const QString &outputDir, const QString &baseName) const;

    /// If path is empty / taken, suggest a unique name; otherwise return cleaned base.
    Q_INVOKABLE QString suggestProjectName(const QString &outputDir, const QString &desiredName) const;

    /// True if outputDir/name already exists and is non-empty.
    Q_INVOKABLE bool projectExists(const QString &outputDir, const QString &name) const;

signals:
    void kitsChanged();
    void qtRootChanged();
    void md3PathChanged();
    void lastErrorChanged();
    void lastOutputChanged();
    void busyChanged();
    void generated(const QString &path);
    void failed(const QString &message);

private:
    void setError(const QString &msg);
    void setBusy(bool on);
    static QString cmakePath(const QString &native);
    static QString render(const QString &text, const QMap<QString, QString> &vars);
    bool writeRendered(const QString &qrcPath, const QString &destFile,
                       const QMap<QString, QString> &vars);
    bool writeBytes(const QString &qrcPath, const QString &destFile);
    bool copyMd3IntoProject(const QString &srcRoot, const QString &destVendorDir);

    QVariantList m_kits;
    QString m_qtRoot;
    QString m_md3Path;
    QString m_lastError;
    QString m_lastOutput;
    bool m_busy = false;
};
