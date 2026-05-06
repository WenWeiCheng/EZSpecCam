---
name: qt-test-runner
description: Execute and debug Qt/C++ unit tests efficiently. Use when running QTest-based unit tests, debugging test failures, or analyzing test output. Handles test execution patterns, output handling, and cross-platform issues.
---

# Qt Test Runner

## Core Principle

Qt tests are Windows executables that cannot run directly under WSL. Always use `cmd.exe` to execute test binaries.

## Execution Patterns

### Pattern 1: From WSL to Windows Executable
```bash
# Option 1: Direct Windows path
/mnt/c/path/to/project/build/.../debug/test_name.exe

# Option 2: Via cmd.exe
cmd.exe /c "C:\\path\\to\\project\\build\\...\\test_name.exe"

# Option 3: Using wslpath for path translation
cmd.exe /c "$(wslpath -w ./test_name.exe)"
```

### Pattern 2: Finding Test Executables
Test binaries are typically in build output directories:
```
build/
├── [config]/
│   └── tests/
│       ├── test_name1/
│       │   └── debug/
│       │       └── test_name1.exe
│       └── test_name2/
│           └── debug/
│               └── test_name2.exe
```

Find them:
```bash
# Use glob to find .exe files in build directory
find /mnt/c/path/to/project/build -name "*.exe" -type f 2>/dev/null | grep test
```

### Pattern 3: Build System Integration
Most Qt projects use qmake + make or CMake. Common patterns:

**qmake with build srcipt**:
```bash
# From Windows
cmd.exe /c "C:\\path\\to\\project\\build_debug.bat"

# Or manually
cd build/.../tests/test_name
make
```

**CMake**:
```bash
cmd.exe /c "cmake --build C:\\path\\to\\build --target test_name"
```

## Output Handling

### Problem: Output Truncation
Test output can exceed tool limits (typically 50k-100k bytes).

**Solution**: Filter immediately with grep
```bash
# Get only test results (PASS/FAIL lines)
./test.exe 2>&1 | grep -E "^(PASS|FAIL|SKIP)"

# Get summary counts
./test.exe 2>&1 | grep -c "^PASS"
./test.exe 2>&1 | grep -c "^FAIL"

# Show last N results
./test.exe 2>&1 | grep -E "^(PASS|FAIL)" | tail -20
```

### Verbose Modes
| Flag | Purpose |
|------|---------|
| `-v1` | Log test function names |
| `-v2` | Log each QVERIFY/QCOMPARE |
| `-vs` | Log every signal emission |
| `-silent` | Failures only |

### Run Specific Test
```bash
./test.exe test_function_name -v2
```

## Debugging Failed Tests

### Step 1: Locate Failure
```bash
./test.exe 2>&1 | grep "^FAIL"
```

### Step 2: Extract Context
```bash
./test.exe test_name -v2 2>&1 | head -80
```

### Step 3: Find Root Cause
```bash
# Filter for relevant keywords
./test.exe test_name -v2 -vs 2>&1 | grep -E "(param|error|FAIL)" | head -20

# Show surrounding context
./test.exe test_name -v2 -vs 2>&1 | grep -B10 "FAIL"
```

### Step 4: Check Signal Flow (for state machine issues)
```bash
./test.exe test_name -vs 2>&1 | grep "Signal:" | head -30
```

## Cross-Platform Patterns

### WSL + Windows Toolchain
```
┌─────────────────┐     ┌─────────────────┐
│   WSL (zsh)    │────▶│  cmd.exe /c     │
│   (workspace)  │     │  (execute)      │
└─────────────────┘     └─────────────────┘
         │                        │
         │ wslpath -w            │
         ▼                        ▼
   /mnt/c/...              C:\Users\...
```

### Build Tool Pattern
```bash
# Generic pattern for WSL with Windows builds
cmd.exe /c "$(wslpath -w /mnt/c/path/to/build_script.bat)"
```

## Methodology: Finding Project Info

When working with unknown Qt projects:

### 1. Find Build Configuration
```bash
# Look for build scripts
ls /mnt/c/path/to/project/*.bat  # Windows
ls /mnt/c/path/to/project/*.sh   # Linux

# Look for project files
find /mnt/c/path/to/project -name "*.pro" -o -name "CMakeLists.txt" | head -10
```

### 2. Find Test Locations
```bash
# Look in src/tests or tests directory
find /mnt/c/path/to/project -type d -name "test*" | head -10

# Look for test .pro files
find /mnt/c/path/to/project -name "test*.pro" | head -10
```

### 3. Find Test Executables
```bash
# After building, find .exe files
find /mnt/c/path/to/project/build -name "test*.exe" 2>/dev/null
```

## Common Test Patterns

### Pattern: Config File Cleanup
Tests sharing config files need isolation:
```cpp
void init() {
    // Clean up any existing config/state files
    QString configPath = getConfigPath("camera-id");
    if (QFile::exists(configPath)) {
        QFile::remove(configPath);
    }
}
```

### Pattern: Test Fixtures
Qt test uses init()/cleanup() slots:
```cpp
class TestClass : public QObject {
    Q_OBJECT
private slots:
    void init() { /* setup */ }
    void cleanup() { /* teardown */ }
    void test_something() { /* test */ }
};
```

## Quick Reference

| Task | Command |
|------|---------|
| Run test | `cmd.exe /c "C:\\path\\to\\test.exe"` |
| Find tests | `find /path/build -name "test*.exe"` |
| Filter output | `grep -E "^(PASS\|FAIL\|SKIP)"` |
| Count passes | `grep -c "^PASS"` |
| Single test | `./test.exe test_name` |
| Verbose | `-v2` |
| Debug signals | `-vs` |

## Anti-Patterns

1. **Don't run directly in WSL**: Qt test binaries are Windows PE executables
2. **Don't read full output**: Use grep to filter, full output is 100k+ bytes
3. **Don't assume test paths**: Always discover from project structure
4. **Don't skip cleanup**: Shared state between tests causes flaky failures
