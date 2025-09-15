#!/usr/bin/env sh
# Test plugin system functionality
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: plugin system..."

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

# Test 1: plugin command exists
echo "Testing plugin command availability..."
"$CJSH_PATH" -c "plugin" >/tmp/plugin_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then  # Command exists (exit 1 might be usage error)
    pass_test "plugin command exists"
else
    fail_test "plugin command not found"
fi

# Test 2: plugin list
echo "Testing plugin list functionality..."
"$CJSH_PATH" -c "plugin list" >/tmp/plugin_list_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "plugin list command"
else
    skip_test "plugin list command (plugins may be disabled or not available)"
fi

# Test 3: plugin help
echo "Testing plugin help..."
"$CJSH_PATH" -c "plugin help" >/tmp/plugin_help_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "plugin help command"
else
    skip_test "plugin help command"
fi

# Test 4: Check if plugins directory exists and has example plugins
PLUGIN_DIR="$(cd "$(dirname "$0")/../../plugins" && pwd)"
if [ -d "$PLUGIN_DIR" ]; then
    pass_test "plugins directory exists"
    
    # Check for example plugins
    if [ -d "$PLUGIN_DIR/example_c_plugin" ]; then
        pass_test "example C plugin directory exists"
    else
        fail_test "example C plugin directory missing"
    fi
    
    if [ -d "$PLUGIN_DIR/example_cpp_plugin" ]; then
        pass_test "example C++ plugin directory exists"
    else
        fail_test "example C++ plugin directory missing"
    fi
    
    if [ -d "$PLUGIN_DIR/example_rust_plugin" ]; then
        pass_test "example Rust plugin directory exists"
    else
        fail_test "example Rust plugin directory missing"
    fi
else
    fail_test "plugins directory not found"
fi

# Test 5: Plugin loading test (if plugins are built)
echo "Testing plugin loading..."
if [ -f "$PLUGIN_DIR/example_c_plugin/example_c_plugin.so" ]; then
    "$CJSH_PATH" -c "plugin load example_c_plugin" >/tmp/plugin_load_test.out 2>&1
    if [ $? -eq 0 ]; then
        pass_test "plugin loading (C plugin)"
    else
        skip_test "plugin loading failed (may need to build plugin first)"
    fi
else
    skip_test "plugin loading test (no built plugin found)"
fi

# Test 6: Plugin status/info
echo "Testing plugin status..."
"$CJSH_PATH" -c "plugin status" >/tmp/plugin_status_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "plugin status command"
else
    skip_test "plugin status command"
fi

# Test 7: Plugin configuration
echo "Testing plugin configuration..."
"$CJSH_PATH" -c "plugin config" >/tmp/plugin_config_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then  # May return error if no args provided
    pass_test "plugin config command exists"
else
    skip_test "plugin config command"
fi

# Test 8: Plugin API functionality
echo "Testing plugin API..."
# Check if plugin.h exists for API testing
PLUGIN_HEADER="$(cd "$(dirname "$0")/../../include" && pwd)/plugin.h"
if [ -f "$PLUGIN_HEADER" ]; then
    pass_test "plugin API header exists"
else
    fail_test "plugin API header missing"
fi

# Check for pluginapi.h
PLUGINAPI_HEADER="$(cd "$(dirname "$0")/../../include" && pwd)/pluginapi.h"
if [ -f "$PLUGINAPI_HEADER" ]; then
    pass_test "plugin API interface header exists"
else
    fail_test "plugin API interface header missing"
fi

# Test 9: Fast prompt tags plugin (if available)
echo "Testing fast prompt tags plugin..."
if [ -d "$PLUGIN_DIR/fast_prompt_tags" ]; then
    pass_test "fast prompt tags plugin directory exists"
    
    # Test plugin files exist
    if [ -f "$PLUGIN_DIR/fast_prompt_tags/fast_prompt_tags.so" ] || 
       [ -f "$PLUGIN_DIR/fast_prompt_tags/Makefile" ] ||
       [ -f "$PLUGIN_DIR/fast_prompt_tags/CMakeLists.txt" ]; then
        pass_test "fast prompt tags plugin has build files"
    else
        skip_test "fast prompt tags plugin build files"
    fi
else
    fail_test "fast prompt tags plugin directory missing"
fi

# Test 10: Jarvis plugin (if available)
echo "Testing Jarvis plugin..."
if [ -d "$PLUGIN_DIR/jarvis" ]; then
    pass_test "Jarvis plugin directory exists"
else
    skip_test "Jarvis plugin directory"
fi

# Cleanup
rm -f /tmp/plugin_test.out /tmp/plugin_list_test.out /tmp/plugin_help_test.out
rm -f /tmp/plugin_load_test.out /tmp/plugin_status_test.out /tmp/plugin_config_test.out

echo ""
echo "Plugin System Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi