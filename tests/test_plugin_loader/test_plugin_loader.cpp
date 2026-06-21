#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "app/PluginLoader.h"

class TestPluginLoader : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void scan_extra_roots_finds_plugin();
    void find_by_camera_returns_entry();
    void enumerate_cameras_returns_ids();
    void unload_all_clears_entries();
    void missing_root_does_not_crash();
    void empty_dir_returns_zero();
    void scan_default_roots_includes_default_paths();

private:
    QTemporaryDir m_tempDir;
    QString m_pluginDir;
};

void TestPluginLoader::initTestCase() {
    QVERIFY(m_tempDir.isValid());
#ifndef MOCK_PLUGIN_PATH
    QSKIP("MOCK_PLUGIN_PATH not defined at compile time");
#else
    const QString srcPath = QString::fromLocal8Bit(MOCK_PLUGIN_PATH);
    const QString suffix = QFileInfo(srcPath).suffix();
    m_pluginDir = m_tempDir.path() + "/plugins";
    QDir().mkpath(m_pluginDir);
    const QString dst = m_pluginDir + "/mock_camera_driver." + suffix;
    QVERIFY(QFile::copy(srcPath, dst));
#endif
}

void TestPluginLoader::cleanupTestCase() {
    app::plugins::unloadAll();
    app::plugins::setExtraRoots({});
}

void TestPluginLoader::scan_extra_roots_finds_plugin() {
    app::plugins::unloadAll();
    int loaded = app::plugins::scan({ m_pluginDir });
    QVERIFY(loaded >= 1);
    QVERIFY(app::plugins::entries().size() >= 1);
}

void TestPluginLoader::find_by_camera_returns_entry() {
    app::plugins::unloadAll();
    app::plugins::scan({ m_pluginDir });
    const app::plugins::Entry *e = app::plugins::findByCamera("mock-001");
    QVERIFY(e != nullptr);
    QVERIFY(e->cameraIds.contains("mock-001"));
    QVERIFY(!e->filePath.isEmpty());
    QVERIFY(e->instance != nullptr);
}

void TestPluginLoader::enumerate_cameras_returns_ids() {
    app::plugins::unloadAll();
    app::plugins::scan({ m_pluginDir });
    QStringList ids = app::plugins::enumerateCameras();
    QVERIFY(ids.contains("mock-001"));
}

void TestPluginLoader::unload_all_clears_entries() {
    app::plugins::unloadAll();
    app::plugins::scan({ m_pluginDir });
    QVERIFY(!app::plugins::entries().isEmpty());
    app::plugins::unloadAll();
    QVERIFY(app::plugins::entries().isEmpty());
}

void TestPluginLoader::missing_root_does_not_crash() {
    app::plugins::unloadAll();
    int loaded = app::plugins::scan({ "Z:/nonexistent/path/that/does/not/exist" });
    QCOMPARE(loaded, 0);
    QVERIFY(app::plugins::entries().isEmpty());
}

void TestPluginLoader::empty_dir_returns_zero() {
    app::plugins::unloadAll();
    const QString emptyDir = m_tempDir.path() + "/empty";
    QDir().mkpath(emptyDir);
    int loaded = app::plugins::scan({ emptyDir });
    QCOMPARE(loaded, 0);
    QVERIFY(app::plugins::entries().isEmpty());
}

void TestPluginLoader::scan_default_roots_includes_default_paths() {
    app::plugins::unloadAll();
    app::plugins::setExtraRoots({ m_pluginDir });
    int loaded = app::plugins::scanDefaultRoots();
    QVERIFY(loaded >= 1);
    QVERIFY(app::plugins::findByCamera("mock-001") != nullptr);
}

QTEST_MAIN(TestPluginLoader)
#include "test_plugin_loader.moc"
