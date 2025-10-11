# Semantic Theme System Documentation

## Overview

The semantic theme system is a color management approach that prioritizes **meaning over context**. Instead of defining colors for specific UI components (like `tab_active` or `file_browser_selected`), it defines colors based on their semantic purpose (like `state_active` or `ui_info`). This makes themes more maintainable, consistent, and flexible.

## Core Philosophy

**Traditional approach:**

```yaml
tab_active: "#58A6FF"
input_focus: "#58A6FF"
search_current: "#58A6FF"
# Problem: Repetition and hard to maintain consistency
```

**Semantic approach:**

```yaml
state_active: "#58A6FF"
# Used for: active tabs, focused inputs, current search match, etc.
```

## Color Categories

### Special Background Handling

The `background` color accepts two types of values:

- **`"transparent"`**: Uses the terminal's native background color (ncurses -1 color pair). This allows terminal transparency effects, background images, and custom terminal themes to show through. This is the recommended default for most themes as it respects user terminal customization.
- **Hex color** (e.g., `"#0D1117"`): Uses the specified color as an opaque background, overriding the terminal's background. Use this when your theme requires a specific background color for its color scheme to work correctly.

**Example:**

```yaml
background: "transparent"  # Uses terminal background
# OR
background: "#0D1117"      # Uses specific dark gray
```

### 1. Base Colors

These define the foundational visual appearance:

- **`background`**: Main editor/window background (see [Special Background Handling](#special-background-handling))
- **`foreground`**: Default text color

### 2. State Colors

Colors that represent UI element states, reusable across **all** components:

| Color            | Purpose                              | Example Usage                                        |
| ---------------- | ------------------------------------ | ---------------------------------------------------- |
| `state_active`   | Currently focused or active element  | Active tab, focused input, current selection         |
| `state_selected` | Selected but not necessarily focused | Selected file in browser, highlighted search results |
| `state_hover`    | Hover or highlighted state           | Current line, mouse hover effects                    |
| `state_disabled` | Disabled or dimmed elements          | Hidden files, disabled buttons, inactive features    |

**Key principle**: These states should be used consistently. If something is "active" anywhere in the UI, it uses `state_active`.

### 3. Semantic UI Colors

Colors defined by their meaning or purpose:

| Color          | Meaning                          | Example Usage                                        |
| -------------- | -------------------------------- | ---------------------------------------------------- |
| `ui_primary`   | Primary actions, emphasis        | Links, important buttons, primary highlights         |
| `ui_secondary` | Secondary information            | Metadata, line numbers, less important text          |
| `ui_accent`    | Special or unique elements       | Decorators, special syntax, unique markers           |
| `ui_success`   | Positive, successful, executable | Success messages, executable files, positive actions |
| `ui_warning`   | Caution, modification            | Warning messages, modified file indicators           |
| `ui_error`     | Errors, danger, critical issues  | Error messages, dangerous actions, failures          |
| `ui_info`      | Informational, neutral metadata  | Info messages, directories, neutral indicators       |
| `ui_border`    | Separators and boundaries        | Borders, dividers, panel separators                  |

### 4. Editor-Specific Colors

Colors specific to the code editor:

- **`cursor`**: Text cursor color
- **`line_numbers`**: Default line number color
- **`line_numbers_active`**: Active/current line number color
- **`line_highlight`**: Current line background highlight

### 5. Status Bar

- **`status_bar_bg`**: Status bar background
- **`status_bar_fg`**: Status bar text color
- **`status_bar_active`**: Active/highlighted status bar items

### 6. Syntax Highlighting

Semantic colors for code syntax:

| Token Type  | Color Key        | Purpose                                  |
| ----------- | ---------------- | ---------------------------------------- |
| Keywords    | `keyword`        | Language keywords (if, for, class, etc.) |
| Strings     | `string_literal` | String literals and text                 |
| Numbers     | `number`         | Numeric literals                         |
| Comments    | `comment`        | Code comments                            |
| Functions   | `function_name`  | Function and method names                |
| Variables   | `variable`       | Variable names                           |
| Types       | `type`           | Type names and definitions               |
| Operators   | `operator`       | Operators (+, -, ==, etc.)               |
| Punctuation | `punctuation`    | Brackets, commas, semicolons             |
| Constants   | `constant`       | Constant values                          |
| Namespaces  | `namespace`      | Namespace/module names                   |
| Properties  | `property`       | Object properties                        |
| Decorators  | `decorator`      | Decorators/annotations                   |
| Macros      | `macro`          | Preprocessor macros                      |
| Labels      | `label`          | Labels and goto targets                  |

### 7. Markup/Markdown

Colors for markup languages:

- **`markup_heading`**: Headings
- **`markup_bold`**: Bold text
- **`markup_italic`**: Italic text
- **`markup_code`**: Inline code
- **`markup_code_block`**: Code blocks
- **`markup_link`**: Link text
- **`markup_url`**: URL text
- **`markup_list`**: List items
- **`markup_blockquote`**: Blockquotes
- **`markup_strikethrough`**: Strikethrough text
- **`markup_quote`**: Quoted text

## Usage Guidelines

### File Browser

```yaml
Selected item background: state_selected
Directories: ui_info
Executables: ui_success
Hidden files: state_disabled
Error indicators: ui_error
Hover state: state_hover
```

### Tabs

```yaml
Active tab: state_active
Inactive tabs: ui_secondary
Modified indicator: ui_warning
Hover state: state_hover
```

### Search

```yaml
Match highlight: state_selected
Current match: state_active
Search input focus: state_active
```

### Command Line

```yaml
Prompt: ui_primary
Error messages: ui_error
Success messages: ui_success
Info messages: ui_info
```

### Error/Diagnostic List

```yaml
Errors: ui_error
Warnings: ui_warning
Info/hints: ui_info
Selected diagnostic: state_selected
```

## Creating a Theme

### Basic Structure

```yaml
# Theme metadata
name: "Your Theme Name"

# Core colors
background: "transparent" # or "#XXXXXX" for specific background
foreground: "#XXXXXX"

# State colors
state_active: "#XXXXXX"
state_selected: "#XXXXXX"
state_hover: "#XXXXXX"
state_disabled: "#XXXXXX"

# Semantic UI colors
ui_primary: "#XXXXXX"
ui_secondary: "#XXXXXX"
# ... etc
```

### Design Tips

1. **Start with state colors**: These are your foundation. Choose colors that clearly differentiate between active, selected, hover, and disabled states.

2. **Semantic consistency**:

   - `ui_success` should always feel positive (greens, blues)
   - `ui_error` should always feel critical (reds, oranges)
   - `ui_warning` should feel cautionary (yellows, oranges)
   - `ui_info` should feel neutral (blues, cyans)

3. **Contrast**: Ensure sufficient contrast between `foreground` and `background` for readability. Note that if using `background: "transparent"`, test with various terminal backgrounds to ensure your `foreground` color works well in different scenarios.

4. **Test across contexts**: Your `state_active` will be used in many places—make sure it works everywhere.

5. **Hierarchy**: `ui_primary` should stand out more than `ui_secondary`.

### Example: GitHub Dark Theme

The provided example demonstrates these principles:

- **Active elements** (`state_active: #58A6FF`) use a bright blue that stands out
- **Selected items** (`state_selected: #264F78`) use a darker blue to differentiate from active
- **Hover** (`state_hover: #161B22`) uses a subtle background change
- **Success** (`ui_success: #7EE787`) uses green
- **Errors** (`ui_error: #FF7B72`) uses red
- **Warnings** (`ui_warning: #E3B341`) uses yellow/gold

## Benefits of Semantic Theming

1. **Consistency**: The same semantic color is used everywhere it applies, creating a cohesive UI.

2. **Maintainability**: Change `state_active` once, and all active elements update.

3. **Flexibility**: New UI components automatically inherit appropriate colors based on their semantic purpose.

4. **Clarity**: Designers and developers understand intent (e.g., "this is an error") rather than context (e.g., "this is the color for error list items").

5. **Accessibility**: Semantic meaning helps ensure appropriate contrast and color choices.

## Migration from Component-Specific Themes

If you have an existing theme with component-specific colors:

1. Group similar colors by their semantic purpose
2. Identify which colors represent states vs. meanings
3. Map component colors to semantic equivalents
4. Test the new theme across all UI contexts

Example mapping:

```
tab_active → state_active
file_selected → state_selected
error_message → ui_error
directory_color → ui_info
```

## Future-Proofing

As new UI components are added to the editor, they should:

1. Use existing semantic colors whenever possible
2. Only introduce new semantic colors if truly needed
3. Document the semantic meaning clearly
4. Apply the color consistently across similar contexts

This ensures themes remain compatible and consistent even as the application evolves.
