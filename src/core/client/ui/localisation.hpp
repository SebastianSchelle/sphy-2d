#ifndef LOCALISATION_HPP
#define LOCALISATION_HPP

#include <std-inc.hpp>
#include <unicode/msgfmt.h>

namespace ui
{

struct LocalizedMessage
{
    icu::MessageFormat format;
};

struct LocalizationToken
{
    std::string key;
    std::unordered_map<std::string, icu::Formattable> arguments;
};

class Localisation
{
  public:
    Localisation() {}
    ~Localisation() {}
    bool loadLocaleFile(const string& path, const string& locale);
    int translateTokenString(string& translated, const string& input);

  private:
    std::string formatLocalizedString(const LocalizationToken& token);
    LocalizationToken parseLocalizationToken(std::string_view token);

    std::unordered_map<std::string, LocalizedMessage> messages;
};

}  // namespace ui

#endif