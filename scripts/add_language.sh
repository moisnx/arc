#!/bin/bash
# Helper script to add a new Tree-sitter language parser

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <language_name> <github_url> [enabled]"
    echo ""
    echo "Examples:"
    echo "  $0 java https://github.com/tree-sitter/tree-sitter-java"
    echo "  $0 ruby https://github.com/tree-sitter/tree-sitter-ruby false"
    echo ""
    exit 1
fi

LANG_NAME=$1
GITHUB_URL=$2
ENABLED=${3:-true}  # Default to enabled

REPO_NAME=$(basename $GITHUB_URL .git)
GRAMMARS_DIR="deps/grammars"
MANIFEST_FILE="$GRAMMARS_DIR/manifest.yaml"

# Validate inputs
if [[ ! "$LANG_NAME" =~ ^[a-z_]+$ ]]; then
    echo "ERROR: Language name must be lowercase letters and underscores only"
    exit 1
fi

if [ ! -d "$GRAMMARS_DIR" ]; then
    echo "ERROR: $GRAMMARS_DIR directory not found. Are you in the project root?"
    exit 1
fi

if [ ! -f "$MANIFEST_FILE" ]; then
    echo "ERROR: $MANIFEST_FILE not found"
    exit 1
fi

echo "=== Adding Tree-sitter Parser: $LANG_NAME ==="
echo "Repository: $GITHUB_URL"
echo "Enabled: $ENABLED"
echo ""

# Step 1: Add git submodule
echo "Step 1: Adding git submodule..."
SUBMODULE_PATH="$GRAMMARS_DIR/$REPO_NAME"

if [ -d "$SUBMODULE_PATH" ]; then
    echo "  ⚠️  Submodule already exists at $SUBMODULE_PATH"
else
    git submodule add "$GITHUB_URL" "$SUBMODULE_PATH"
    git submodule update --init --recursive
    echo "  ✓ Submodule added: $SUBMODULE_PATH"
fi

# Step 2: Add to manifest.yaml
echo ""
echo "Step 2: Adding to manifest.yaml..."

# Check if language already in manifest
if grep -q "^  $LANG_NAME:" "$MANIFEST_FILE"; then
    echo "  ⚠️  $LANG_NAME already exists in manifest.yaml"
    echo "  You may need to manually update it"
else
    # Find a good insertion point (before the comments at the end)
    # We'll append before the "# Future features" section
    
    cat >> "$MANIFEST_FILE" << EOF

  $LANG_NAME:
    path: $REPO_NAME
    enabled: $ENABLED
    description: "$LANG_NAME language parser"
EOF
    
    echo "  ✓ Added $LANG_NAME to manifest.yaml"
fi

# Step 3: Verify parser structure
echo ""
echo "Step 3: Verifying parser structure..."
if [ -f "$SUBMODULE_PATH/src/parser.c" ]; then
    echo "  ✓ Found parser.c"
else
    echo "  ⚠️  WARNING: parser.c not found at $SUBMODULE_PATH/src/parser.c"
    echo "     This parser may not build correctly"
fi

if [ -f "$SUBMODULE_PATH/src/scanner.c" ]; then
    echo "  ✓ Found scanner.c"
elif [ -f "$SUBMODULE_PATH/src/scanner.cc" ]; then
    echo "  ✓ Found scanner.cc"
else
    echo "  ℹ️  No scanner file (optional, not all parsers need it)"
fi

# Step 4: Check for queries
echo ""
echo "Step 4: Checking for query files..."
QUERIES_DIR="runtime/queries/$LANG_NAME"
if [ ! -d "$QUERIES_DIR" ]; then
    echo "  ⚠️  Query directory not found: $QUERIES_DIR"
    echo "  You'll need to create queries for syntax highlighting:"
    echo "    mkdir -p $QUERIES_DIR"
    echo "    # Add highlights.scm, etc."
else
    echo "  ✓ Query directory exists: $QUERIES_DIR"
    ls -1 "$QUERIES_DIR"/*.scm 2>/dev/null | sed 's/^/    /' || echo "    (no .scm files yet)"
fi

# Step 5: Update languages.yaml
echo ""
echo "Step 5: Adding to runtime/languages.yaml..."
LANGUAGES_YAML="runtime/languages.yaml"
if [ ! -f "$LANGUAGES_YAML" ]; then
    echo "  ⚠️  $LANGUAGES_YAML not found"
else
    if grep -q "^  $LANG_NAME:" "$LANGUAGES_YAML"; then
        echo "  ℹ️  $LANG_NAME already in languages.yaml"
    else
        cat >> "$LANGUAGES_YAML" << EOF

  $LANG_NAME:
    name: "$LANG_NAME"
    extensions: [".$LANG_NAME"]  # TODO: Update with correct extensions
    parser_name: "$LANG_NAME"
    builtin: true
    queries:
      - "$LANG_NAME/highlights.scm"
EOF
        echo "  ✓ Added $LANG_NAME to languages.yaml"
        echo "  ⚠️  Remember to update the 'extensions' field with correct values!"
    fi
fi

echo ""
echo "=== Summary ==="
echo "✓ Submodule added: $SUBMODULE_PATH"
echo "✓ Updated manifest.yaml"
echo ""
echo "Next steps:"
echo "1. Create query files in runtime/queries/$LANG_NAME/"
echo "   - highlights.scm (required)"
echo "   - injections.scm (optional)"
echo "2. Update runtime/languages.yaml extensions for .$LANG_NAME files"
echo "3. Rebuild the project:"
echo "   cmake --build build"
echo "4. Test the parser:"
echo "   ./build/arc test.$LANG_NAME"
echo "5. Commit your changes:"
echo "   git add ."
echo "   git commit -m 'Add $LANG_NAME language support'"
echo ""