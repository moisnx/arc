#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Starting reorganization in test_examples"

ROOT="test_examples"

# Safety: ensure directory exists
if [[ ! -d "$ROOT" ]]; then
  echo "❌ Error: $ROOT not found."
  exit 1
fi

# Define language and meta directories
LANG_DIRS=("bash" "c" "cpp" "go" "html" "javascript" "markdown" "python" "rust" "typescript" "zig" "_meta")

# Create directories if missing
for dir in "${LANG_DIRS[@]}"; do
  mkdir -p "$ROOT/$dir"
done

# Move known misc/meta files to _meta
for f in "$ROOT"/*; do
  base=$(basename "$f")
  case "$base" in
    *.md|*.ini|simpletest.txt|essay.*|debugnew.*|migration.*)
      echo "📦 Moving $base → _meta/"
      mv -f "$f" "$ROOT/_meta/" 2>/dev/null || true
      ;;
  esac
done

# Move language files based on extensions
declare -A EXT_MAP=(
  ["sh"]="bash"
  ["c"]="c"
  ["h"]="c"
  ["cpp"]="cpp"
  ["hpp"]="cpp"
  ["go"]="go"
  ["html"]="html"
  ["js"]="javascript"
  ["ts"]="typescript"
  ["py"]="python"
  ["rs"]="rust"
  ["zig"]="zig"
  ["md"]="markdown"
)

# Traverse files
find "$ROOT" -maxdepth 1 -type f | while read -r file; do
  ext="${file##*.}"
  base=$(basename "$file")
  target="${EXT_MAP[$ext]:-_meta}"

  # Skip already organized files
  if [[ "$file" == "$ROOT/$target/"* ]]; then
    continue
  fi

  echo "📦 Moving $base → $target/"
  mkdir -p "$ROOT/$target"
  mv -f "$file" "$ROOT/$target/" 2>/dev/null || true
done

# Handle known problem cases cleanly
# Rename files/folders that conflict with directory names
if [[ -f "$ROOT/migration.txt" ]]; then
  mv "$ROOT/migration.txt" "$ROOT/migration_text.txt"
fi

if [[ -d "$ROOT/zig/test" ]]; then
  mv "$ROOT/zig/test" "$ROOT/zig/test_file"
fi

if [[ -f "$ROOT/bash/clean-old.sh" ]]; then
  echo "✅ bash/clean-old.sh already exists, skipping duplicate creation"
fi

echo "✅ Reorganization complete!"
