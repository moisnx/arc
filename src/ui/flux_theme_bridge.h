// src/ui/flux_theme_bridge.h
// Bridge between Arc's StyleManager and Flux's theme system
#pragma once

#include "style_manager.h"
#include <flux/ui/theme.h>
#include <iostream>

namespace arc
{

class FluxThemeBridge
{
public:
  // Convert Arc's StyleManager state to a Flux theme
  // Returns COLOR_PAIR indices that point to Arc's already-initialized pairs
  static flux::Theme createFluxThemeFromArc()
  {
    flux::Theme theme;

    // CRITICAL: These are NOT new color pairs - they're indices into
    // Arc's existing COLOR_PAIR() table (0-79)

    // Map Arc's semantic pairs to what Flux expects
    theme.background = ColorPairs::BACKGROUND_PAIR;          // 1
    theme.foreground = ColorPairs::FOREGROUND_PAIR;          // 2
    theme.selected = ColorPairs::STATE_SELECTED;             // 11
    theme.directory = ColorPairs::UI_INFO;                   // 26
    theme.executable = ColorPairs::UI_SUCCESS;               // 23
    theme.hidden = ColorPairs::STATE_DISABLED;               // 13
    theme.symlink = ColorPairs::UI_ACCENT;                   // 22
    theme.parent_dir = ColorPairs::UI_PRIMARY;               // 20
    theme.status_bar = ColorPairs::STATUS_BAR;               // 40
    theme.status_bar_active = ColorPairs::STATUS_BAR_ACTIVE; // 42
    theme.ui_secondary = ColorPairs::UI_SECONDARY;           // 21
    theme.ui_border = ColorPairs::UI_BORDER;                 // 27
    theme.ui_error = ColorPairs::UI_ERROR;                   // 25
    theme.ui_warning = ColorPairs::UI_WARNING;               // 24
    theme.ui_accent = ColorPairs::UI_ACCENT;                 // 22
    theme.ui_info = ColorPairs::UI_INFO;                     // 26
    theme.ui_success = ColorPairs::UI_SUCCESS;               // 23

    return theme;
  }

  // Debug: Print what color pairs are being used
  static void debugPrintMapping()
  {
    std::cerr << "\n=== Flux Theme Bridge Mapping ===" << std::endl;
    std::cerr << "background:        COLOR_PAIR(" << ColorPairs::BACKGROUND_PAIR
              << ")" << std::endl;
    std::cerr << "foreground:        COLOR_PAIR(" << ColorPairs::FOREGROUND_PAIR
              << ")" << std::endl;
    std::cerr << "selected:          COLOR_PAIR(" << ColorPairs::STATE_SELECTED
              << ")" << std::endl;
    std::cerr << "directory:         COLOR_PAIR(" << ColorPairs::UI_INFO << ")"
              << std::endl;
    std::cerr << "executable:        COLOR_PAIR(" << ColorPairs::UI_SUCCESS
              << ")" << std::endl;
    std::cerr << "status_bar:        COLOR_PAIR(" << ColorPairs::STATUS_BAR
              << ")" << std::endl;
    std::cerr << "================================\n" << std::endl;
  }
};

} // namespace arc