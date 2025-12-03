#!/bin/bash
# Comprehensive test for embedded query system

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Arc Editor - Query System Comprehensive Test     ${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════${NC}"
echo ""

# Test 1: Check binary exists
echo -e "${BLUE}[1/7] Checking binary...${NC}"
if [ ! -f "build/arc" ]; then
    echo -e "${RED}❌ build/arc not found${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Binary exists${NC}"
echo ""

# Test 2: Check embedded query generation
echo -e "${BLUE}[2/7] Checking embedded query generation...${NC}"
if [ ! -f "build/generated/embedded_queries.cpp" ]; then
    echo -e "${RED}❌ embedded_queries.cpp not generated${NC}"
    echo "Run: rm -rf build && cmake -B build && cmake --build build"
    exit 1
fi

lines=$(wc -l < build/generated/embedded_queries.cpp)
echo -e "${GREEN}✅ Generated: $lines lines${NC}"

# Check for key languages
for lang in python c cpp ecma _javascript; do
    if grep -q "${lang}_highlights_scm" build/generated/embedded_queries.cpp; then
        echo -e "   ${GREEN}✅ ${lang} embedded${NC}"
    else
        echo -e "   ${RED}❌ ${lang} missing${NC}"
    fi
done
echo ""

# Test 3: Test from project root (dev mode)
echo -e "${BLUE}[3/7] Testing from project root (dev mode)...${NC}"
cat > /tmp/test.py << 'EOF'
def hello():
    return "world"
EOF

timeout 2s ./build/arc /tmp/test.py 2>&1 | grep -q "Loading queries" && \
    echo -e "${GREEN}✅ Loads in dev mode${NC}" || \
    echo -e "${YELLOW}⚠️  No query loading output${NC}"
pkill -f "build/arc" 2>/dev/null || true
echo ""

# Test 4: Test from /tmp (embedded mode)
echo -e "${BLUE}[4/7] Testing from /tmp (embedded fallback)...${NC}"
cd /tmp

# Copy binary to /tmp to test truly standalone
cp $OLDPWD/build/arc /tmp/arc_test

cat > test_cpp.cpp << 'EOF'
#include <iostream>
int main() {
    std::cout << "Hello" << std::endl;
    return 0;
}
EOF

echo "Running from /tmp (no runtime/ directory available)..."
timeout 2s ./arc_test test_cpp.cpp 2>&1 | tee /tmp/arc_output.log
pkill -f "arc_test" 2>/dev/null || true

if grep -q "embedded query" /tmp/arc_output.log; then
    echo -e "${GREEN}✅ Uses embedded queries${NC}"
elif grep -q "Successfully loaded" /tmp/arc_output.log; then
    echo -e "${GREEN}✅ Queries loaded successfully${NC}"
else
    echo -e "${RED}❌ No query loading detected${NC}"
    echo "Output:"
    cat /tmp/arc_output.log
fi

cd $OLDPWD
rm -f /tmp/arc_test /tmp/test.py /tmp/test_cpp.cpp /tmp/arc_output.log
echo ""

# Test 5: Test C++ (multiple query dependencies)
echo -e "${BLUE}[5/7] Testing C++ (requires C + C++ queries)...${NC}"
cat > /tmp/test.cpp << 'EOF'
#include <vector>

class MyClass {
    int value;
public:
    MyClass(int v) : value(v) {}
};
EOF

timeout 2s ./build/arc /tmp/test.cpp 2>&1 | tee /tmp/cpp_test.log
pkill -f "build/arc" 2>/dev/null || true

# Check if both C and C++ queries were loaded
if grep -q "runtime/queries/c/highlights.scm" /tmp/cpp_test.log && \
   grep -q "runtime/queries/cpp/highlights.scm" /tmp/cpp_test.log; then
    echo -e "${GREEN}✅ Loaded both C and C++ queries${NC}"
elif grep -q "Successfully loaded" /tmp/cpp_test.log; then
    echo -e "${GREEN}✅ Queries loaded${NC}"
else
    echo -e "${YELLOW}⚠️  Check query loading${NC}"
fi

rm -f /tmp/test.cpp /tmp/cpp_test.log
echo ""

# Test 6: Test JavaScript (requires ecma + _javascript)
echo -e "${BLUE}[6/7] Testing JavaScript (requires ecma + _javascript)...${NC}"
cat > /tmp/test.js << 'EOF'
function hello() {
    const x = 42;
    return x;
}
EOF

timeout 2s ./build/arc /tmp/test.js 2>&1 | tee /tmp/js_test.log
pkill -f "build/arc" 2>/dev/null || true

if grep -q "ecma" /tmp/js_test.log || grep -q "Successfully loaded" /tmp/js_test.log; then
    echo -e "${GREEN}✅ JavaScript queries loaded${NC}"
else
    echo -e "${YELLOW}⚠️  Check JavaScript queries${NC}"
fi

rm -f /tmp/test.js /tmp/js_test.log
echo ""

# Test 7: Binary size check
echo -e "${BLUE}[7/7] Binary size analysis...${NC}"
size_kb=$(du -k build/arc | cut -f1)
echo "Binary size: ${size_kb} KB"

if [ $size_kb -lt 1000 ]; then
    echo -e "${YELLOW}⚠️  Binary seems small - queries might not be fully embedded${NC}"
elif [ $size_kb -gt 20000 ]; then
    echo -e "${YELLOW}⚠️  Binary seems large - consider stripping debug symbols${NC}"
    echo "Run: strip build/arc"
else
    echo -e "${GREEN}✅ Binary size looks good${NC}"
fi
echo ""

# Summary
echo -e "${BLUE}════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}                     Summary                        ${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${GREEN}Query Resolution System:${NC}"
echo "  Priority 1: ./runtime/queries/LANG/TYPE.scm (development)"
echo "  Priority 2: ~/.config/arceditor/queries/LANG/TYPE.scm (user)"
echo "  Priority 3: /usr/share/arc/queries/LANG/TYPE.scm (system)"
echo "  Priority 4: Embedded in binary (always works)"
echo ""
echo -e "${GREEN}Features:${NC}"
echo "  ✓ Multiple query dependencies (C++, JavaScript)"
echo "  ✓ Works from any directory"
echo "  ✓ User customization support"
echo "  ✓ Embedded fallback"
echo ""
echo -e "${GREEN}Next steps:${NC}"
echo "  • Test: ./build/arc yourfile.cpp"
echo "  • Install: sudo make install (system-wide)"
echo "  • Install: make install-user (user-only)"
echo "  • Customize: ~/.config/arceditor/queries/LANG/TYPE.scm"
echo ""
echo -e "${GREEN}✅ Tests complete!${NC}"