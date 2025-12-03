// Bridge file to break circular dependency

#include "injection_manager.h"
#include "syntax_config_loader.h"
#include "syntax_highlighter.h"

#ifdef TREE_SITTER_ENABLED

const TSLanguage *
InjectionManager::getLanguageForInjection(const std::string &lang_name) const
{
  const LanguageConfig *config = getLanguageConfigFromParent(lang_name);

  if (!config || config->parser_name.empty())
    return nullptr;

  if (!parent_highlighter_)
    return nullptr;

  return parent_highlighter_->getLanguageFunction(config->parser_name);
}

int InjectionManager::getColorPairForCapture(
    const std::string &capture_name) const
{
  if (!parent_highlighter_)
    return 0;

  return parent_highlighter_->getColorPairForCapture(capture_name);
}

#endif // TREE_SITTER_ENABLED