#include <QtTest>
#include "app/AppMode.h"

class TestAppMode : public QObject
{
    Q_OBJECT
private slots:
    void no_args_is_windowed();
    void bare_dashdash_is_windowed();
    void list_is_headless();
    void list_params_is_headless();
    void camera_is_headless();
    void set_is_headless();
    void frames_is_headless();
    void sequence_is_headless();
    void output_is_headless();
    void format_is_headless();
    void prefix_is_headless();
    void suffix_is_headless();
    void help_is_headless();
    void version_is_headless();
    void unknown_flag_after_camera_is_headless();
    void case_sensitive_list_is_windowed();
};

void TestAppMode::no_args_is_windowed() {
    int argc = 1;
    char *argv[] = { const_cast<char*>("ezspeccam") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Windowed);
}
void TestAppMode::bare_dashdash_is_windowed() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Windowed);
}
void TestAppMode::list_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--list") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::list_params_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--list-params") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::camera_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--camera"), const_cast<char*>("x") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::set_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--set"), const_cast<char*>("a=1") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::frames_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--frames"), const_cast<char*>("5") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::sequence_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--sequence"), const_cast<char*>("x.json") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::output_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--output"), const_cast<char*>("./") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::format_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--format"), const_cast<char*>("tiff") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::prefix_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--prefix"), const_cast<char*>("p_") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::suffix_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--suffix"), const_cast<char*>("_s") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::help_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--help") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::version_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--version") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::unknown_flag_after_camera_is_headless() {
    int argc = 4;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--camera"),
                     const_cast<char*>("x"), const_cast<char*>("--bogus") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::case_sensitive_list_is_windowed() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--List") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Windowed);
}

QTEST_MAIN(TestAppMode)
#include "test_app_mode.moc"
