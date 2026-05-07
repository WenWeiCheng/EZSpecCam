#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>

#include "CameraTypes.h"

class TestConfigurationManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testValidateFloatRange();
    void testValidateIntRange();
    void testValidateFloatCollection();
    void testValidateIntCollection();
    void testValidateStringCollection();
    void testValidateBoolean();
    void testValidateReason();

private:
    QTemporaryDir m_tempDir;
    QString m_testCameraId;
};

void TestConfigurationManager::initTestCase()
{
    m_testCameraId = "test-camera-001";
}

void TestConfigurationManager::cleanupTestCase()
{
}

void TestConfigurationManager::init()
{
}

void TestConfigurationManager::cleanup()
{
}

void TestConfigurationManager::testValidateFloatRange()
{
    ParameterConstraint constraint;
    constraint.minValue = 0.0;
    constraint.maxValue = 1000.0;

    QVERIFY2(validate(50.0, constraint, ParameterType::FloatRange) == true,
             "Value within range should be valid");
    QVERIFY2(validate(0.0, constraint, ParameterType::FloatRange) == true,
             "Min value should be valid");
    QVERIFY2(validate(1000.0, constraint, ParameterType::FloatRange) == true,
             "Max value should be valid");
    QVERIFY2(validate(-1.0, constraint, ParameterType::FloatRange) == false,
             "Value below range should be invalid");
    QVERIFY2(validate(1001.0, constraint, ParameterType::FloatRange) == false,
             "Value above range should be invalid");
}

void TestConfigurationManager::testValidateIntRange()
{
    ParameterConstraint constraint;
    constraint.minValue = 0;
    constraint.maxValue = 100;

    QVERIFY2(validate(50, constraint, ParameterType::IntRange) == true,
             "Value within range should be valid");
    QVERIFY2(validate(0, constraint, ParameterType::IntRange) == true,
             "Min value should be valid");
    QVERIFY2(validate(100, constraint, ParameterType::IntRange) == true,
             "Max value should be valid");
    QVERIFY2(validate(-1, constraint, ParameterType::IntRange) == false,
             "Value below range should be invalid");
    QVERIFY2(validate(101, constraint, ParameterType::IntRange) == false,
             "Value above range should be invalid");
}

void TestConfigurationManager::testValidateFloatCollection()
{
    ParameterConstraint constraint;
    constraint.validValues = {1.0, 2.0, 4.0, 8.0};

    QVERIFY2(validate(2.0, constraint, ParameterType::FloatCollection) == true,
             "Value in collection should be valid");
    QVERIFY2(validate(3.0, constraint, ParameterType::FloatCollection) == false,
             "Value not in collection should be invalid");
}

void TestConfigurationManager::testValidateIntCollection()
{
    ParameterConstraint constraint;
    constraint.validValues = {1, 2, 4, 8};

    QVERIFY2(validate(4, constraint, ParameterType::IntCollection) == true,
             "Value in collection should be valid");
    QVERIFY2(validate(3, constraint, ParameterType::IntCollection) == false,
             "Value not in collection should be invalid");
}

void TestConfigurationManager::testValidateStringCollection()
{
    ParameterConstraint constraint;
    constraint.validValues = {"mode1", "mode2", "mode3"};

    QVERIFY2(validate("mode2", constraint, ParameterType::StringCollection) == true,
             "Value in collection should be valid");
    QVERIFY2(validate("mode5", constraint, ParameterType::StringCollection) == false,
             "Value not in collection should be invalid");
}

void TestConfigurationManager::testValidateBoolean()
{
    ParameterConstraint constraint;

    QVERIFY2(validate(true, constraint, ParameterType::Boolean) == true,
             "Boolean true should be valid");
    QVERIFY2(validate(false, constraint, ParameterType::Boolean) == true,
             "Boolean false should be valid");
    QVERIFY2(validate(1, constraint, ParameterType::Boolean) == true,
             "Integer 1 should be convertible to boolean");
}

void TestConfigurationManager::testValidateReason()
{
    ParameterConstraint constraint;
    constraint.minValue = 0.0;
    constraint.maxValue = 100.0;

    QString reason = validateReason(-5.0, constraint, ParameterType::FloatRange);
    QVERIFY2(!reason.isEmpty(), "Should return reason for invalid value");
    QVERIFY2(reason.contains("below"), "Reason should mention 'below'");

    reason = validateReason(150.0, constraint, ParameterType::FloatRange);
    QVERIFY2(!reason.isEmpty(), "Should return reason for invalid value");
    QVERIFY2(reason.contains("above"), "Reason should mention 'above'");

    reason = validateReason(50.0, constraint, ParameterType::FloatRange);
    QVERIFY2(reason.isEmpty(), "Valid value should return empty reason");
}

QTEST_MAIN(TestConfigurationManager)
#include "test_configuration.moc"
