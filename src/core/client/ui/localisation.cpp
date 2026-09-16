#include <algorithm>
#include <fstream>
#include <localisation.hpp>
#include <regex>
#include <string>
#include <unicode/locid.h>
#include <unicode/msgfmt.h>
#include <unicode/unistr.h>

namespace ui
{

static const std::regex localizationRegex(
    R"(\[([A-Za-z0-9_.-]+)((?:\|[A-Za-z0-9_.-]+=[^\]|]*)*)\])");

/*


    UErrorCode status = U_ZERO_ERROR;

    LocalizedMessage message{
        icu::MessageFormat("Main Menu", icu::Locale("en"), status)};
    if (!U_FAILURE(status))
    {
        messages.insert_or_assign("menu.title", message);
    }
    LocalizedMessage message2{icu::MessageFormat(
        "{count, plural, =0 {No items} one {# item} other {# items}}",
        icu::Locale("en"),
        status)};
    if (!U_FAILURE(status))
    {
        messages.insert_or_assign("menu.items", message2);
    }

*/

int Localisation::translateTokenString(string& translated, const string& input)
{
    std::string result;

    std::sregex_iterator it(input.begin(), input.end(), localizationRegex);
    const std::sregex_iterator end;

    std::size_t lastPosition = 0;

    for (; it != end; ++it)
    {
        const std::smatch& match = *it;

        // Preserve text before this localization token.
        result.append(input,
                      lastPosition,
                      static_cast<std::size_t>(match.position())
                          - lastPosition);

        const std::string_view tokenText(input.data() + match.position(),
                                         match.length());

        LocalizationToken token = parseLocalizationToken(tokenText);

        result += formatLocalizedString(token);

        lastPosition =
            static_cast<std::size_t>(match.position() + match.length());
    }

    // Preserve trailing text.
    result.append(input, lastPosition, std::string::npos);

    translated = result;
    return 0;
}

std::string Localisation::formatLocalizedString(const LocalizationToken& token)
{
    // 1. Lookup token.key.
    // 2. Convert token.arguments to ICU Formattable arguments.
    // 3. Pass the localized message to ICU MessageFormat.
    // 4. Return the formatted result.
    auto it = messages.find(token.key);
    if (it == messages.end())
    {
        LG_D("token key {} not found in translation table", token.key);
        return token.key;
    }

    std::vector<icu::UnicodeString> argumentNames;
    std::vector<icu::Formattable> argumentValues;

    argumentNames.reserve(token.arguments.size());
    argumentValues.reserve(token.arguments.size());
    for (auto arg : token.arguments)
    {
        argumentNames.emplace_back(icu::UnicodeString::fromUTF8(arg.first));
        argumentValues.emplace_back(arg.second);
    }

    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString result;
    it->second.format.format(argumentNames.data(),
                             argumentValues.data(),
                             argumentNames.size(),
                             result,
                             status);

    if (U_FAILURE(status))
    {
        LG_W("Parsing of {} failed {}", token.key, (int)status);
        return token.key;
    }
    string output;
    result.toUTF8String(output);
    return output;
}

static icu::Formattable toFormattable(std::string_view value)
{
    int32_t integerValue{};

    const auto [ptr, ec] = std::from_chars(
        value.data(), value.data() + value.size(), integerValue);

    if (ec == std::errc{} && ptr == value.data() + value.size())
    {
        return icu::Formattable(integerValue);
    }

    return icu::Formattable(icu::UnicodeString::fromUTF8(
        icu::StringPiece(value.data(), value.size())));
}

LocalizationToken Localisation::parseLocalizationToken(std::string_view token)
{
    std::cmatch match;

    // token must include the [ ... ] delimiters.
    std::string input(token);

    if (!std::regex_match(input.c_str(), match, localizationRegex))
        return {};

    LocalizationToken result;
    result.key = match[1].str();

    const std::string args = match[2].str();

    if (!args.empty())
    {
        // Skip the leading '|'.
        std::size_t pos = 1;

        while (pos < args.size())
        {
            const std::size_t separator = args.find('|', pos);
            const std::size_t end =
                separator == std::string::npos ? args.size() : separator;

            const std::string_view argument(args.data() + pos, end - pos);

            const std::size_t equals = argument.find('=');

            if (equals != std::string_view::npos)
            {
                const std::string name(argument.substr(0, equals));
                const std::string value(argument.substr(equals + 1));

                result.arguments.emplace(name, toFormattable(value));
            }

            if (separator == std::string::npos)
                break;

            pos = separator + 1;
        }
    }

    return result;
}

bool Localisation::loadLocaleFile(const string& path, const string& locale)
{
    std::ifstream ifs(path);
    std::string line;
    std::string key;
    std::string value;

    while (std::getline(ifs, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        const auto equals = line.find('=');

        if (equals != std::string::npos)
        {
            key = line.substr(0, equals);
            trim(key);
            value = line.substr(equals + 1);
            trim(value);

            // Then collect continuation lines...
            auto openCnt = std::ranges::count(value, '{');
            auto closeCnt = std::ranges::count(value, '}');
            while (openCnt != closeCnt && std::getline(ifs, line))
            {
                trim(line);
                openCnt += std::ranges::count(line, '{');
                closeCnt += std::ranges::count(line, '}');
                value += line;
            }

            UErrorCode status = U_ZERO_ERROR;

            LocalizedMessage message{icu::MessageFormat(
                icu::UnicodeString::fromUTF8(value),
                icu::Locale(icu::Locale::createFromName(locale)),
                status)};
            if (U_FAILURE(status))
            {
                LG_W("Could not create LocalizedMessage from {}:{}. Error: {}",
                     key,
                     value,
                     (int)status);
            }
            else
            {
                messages.insert_or_assign(key, message);
            }
        }
    }
    return true;
}

}  // namespace ui