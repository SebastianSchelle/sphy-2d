#include "command-node.hpp"
#include <iterator>
#include <sstream>

namespace cmd
{

CommandArgs CommandManager::parseArgumentTokens(const std::vector<string>& tokens)
{
    CommandArgs r;
    for (size_t i = 0; i < tokens.size();)
    {
        const string& t = tokens[i];
        if (!t.empty() && t[0] == '-')
        {
            const size_t eq = t.find('=');
            if (eq != string::npos)
            {
                r.flags[t.substr(0, eq)] = t.substr(eq + 1);
                ++i;
                continue;
            }
            const string flag = t;
            if (i + 1 < tokens.size() && !tokens[i + 1].empty()
                && tokens[i + 1][0] != '-')
            {
                r.flags[flag] = tokens[i + 1];
                i += 2;
            }
            else
            {
                r.flags[flag] = "";
                ++i;
            }
        }
        else
        {
            r.positionals.push_back(t);
            ++i;
        }
    }
    return r;
}

CommandNode::CommandNode(const string& key) : keyword(key) {}

CommandNode::~CommandNode() {}

const string& CommandNode::getKeyword() const
{
    return keyword;
}

CommandBranch::CommandBranch(const string& key) : CommandNode(key) {}

CommandBranch::~CommandBranch() {}

auto CommandBranch::findChildBranch(const string& kw) const
{
    return std::find_if(
        children.cbegin(),
        children.cend(),
        [&kw](const std::shared_ptr<CommandNode>& node)
        { return node->getKeyword() == kw && !node->isLeaf(); });
}

auto CommandBranch::findChildLeaf(const string& kw) const
{
    return std::find_if(
        children.cbegin(),
        children.cend(),
        [&kw](const std::shared_ptr<CommandNode>& node)
        { return node->getKeyword() == kw && node->isLeaf(); });
}

bool CommandBranch::registerCommand(vector<std::string> commandPath,
                                    CommandCallback callback,
                                    const string& commandHelp,
                                    const std::vector<CommandFlagDef>& flagDefs)
{
    const std::string next = commandPath.back();
    commandPath.pop_back();
    if (commandPath.empty())
    {
        auto it = findChildLeaf(next);
        if (it != children.end())
        {
            return (*it)->registerCommand(commandPath, callback, commandHelp,
                                          flagDefs);
        }
        children.push_back(std::make_shared<CommandLeaf>(
            next, callback, commandHelp, flagDefs));
        return true;
    }

    auto itB = findChildBranch(next);
    if (itB != children.end())
    {
        return (*itB)->registerCommand(commandPath, callback, commandHelp,
                                       flagDefs);
    }

    children.push_back(std::make_shared<CommandBranch>(next));
    return std::static_pointer_cast<CommandBranch>(children.back())
        ->registerCommand(commandPath, callback, commandHelp, flagDefs);
}

string CommandBranch::execute(vector<std::string> commandPath,
                              const CommandArgs& args)
{
    LG_D("Visiting command branch {}, command path: {}",
         keyword.c_str(),
         commandPath);
    if (commandPath.empty())
    {
        return "Failed: Invalid command path";
    }
    const std::string next = commandPath.back();
    commandPath.pop_back();
    if (commandPath.empty())
    {
        auto it = findChildLeaf(next);
        if (it != children.end())
        {
            return (*it)->execute(commandPath, args);
        }
    }
    else
    {
        auto it = findChildBranch(next);
        if (it != children.end())
        {
            return (*it)->execute(commandPath, args);
        }
    }
    return "Failed: Command not found";
}

string CommandBranch::helpSummary(const string& dottedPath) const
{
    return dottedPath.empty() ? keyword : dottedPath;
}

string CommandBranch::helpList(const string& prefix) const
{
    string out;
    for (const auto& child : children)
    {
        const string cmd =
            prefix.empty() ? child->getKeyword() : prefix + "." + child->getKeyword();
        if (child->isLeaf())
        {
            out += child->helpSummary(cmd) + "\n";
        }
        else
        {
            out += std::static_pointer_cast<CommandBranch>(child)->helpList(cmd);
        }
    }
    return out;
}

string CommandBranch::helpNavigate(const std::vector<string>& path,
                                   const string& prefix) const
{
    if (path.empty())
    {
        return helpList(prefix);
    }
    const string& head = path[0];
    vector<string> tail(path.begin() + 1, path.end());
    const string newPrefix =
        prefix.empty() ? head : prefix + "." + head;

    auto itB = findChildBranch(head);
    if (itB != children.end())
    {
        const auto br = std::static_pointer_cast<CommandBranch>(*itB);
        if (tail.empty())
        {
            return br->helpList(newPrefix);
        }
        return br->helpNavigate(tail, newPrefix);
    }
    auto itL = findChildLeaf(head);
    if (itL != children.end())
    {
        const auto lf = std::static_pointer_cast<CommandLeaf>(*itL);
        if (!tail.empty())
        {
            return "Failed: Unknown subcommand";
        }
        return lf->formatFullHelp(newPrefix);
    }
    return "Failed: Unknown command";
}

CommandLeaf::CommandLeaf(const string& key,
                         CommandCallback callback,
                         string commandHelp,
                         std::vector<CommandFlagDef> flagDefs)
    : CommandNode(key),
      callback(std::move(callback)),
      commandHelp(std::move(commandHelp)),
      flagDefs(std::move(flagDefs))
{
}

CommandLeaf::~CommandLeaf() {}

bool CommandLeaf::registerCommand(vector<std::string> commandPath,
                                  CommandCallback callback,
                                  const string& commandHelp,
                                  const std::vector<CommandFlagDef>& flagDefs)
{
    if (!commandPath.empty())
    {
        LG_W("Command leaf {}: cannot register nested path", keyword);
        return false;
    }
    this->callback = std::move(callback);
    this->commandHelp = commandHelp;
    this->flagDefs = flagDefs;
    return true;
}

string CommandLeaf::execute(vector<std::string> commandPath,
                            const CommandArgs& args)
{
    LG_D("Visiting command leaf {}, command path: {}",
         keyword.c_str(),
         commandPath);
    if (!commandPath.empty())
    {
        return "Failed: Invalid command path";
    }
    for (const auto& fd : flagDefs)
    {
        if (fd.required && !args.flags.count(fd.name))
        {
            return "Failed: Missing required flag " + fd.name;
        }
    }
    return callback(args);
}

string CommandLeaf::helpSummary(const string& dottedPath) const
{
    if (commandHelp.empty())
    {
        return dottedPath;
    }
    return dottedPath + " — " + commandHelp;
}

string CommandLeaf::formatFullHelp(const string& dottedPath) const
{
    std::ostringstream o;
    o << dottedPath;
    if (!commandHelp.empty())
    {
        o << " — " << commandHelp;
    }
    o << "\n";
    if (!flagDefs.empty())
    {
        o << "Flags:\n";
        for (const auto& f : flagDefs)
        {
            o << "  " << f.name;
            if (f.required)
            {
                o << " (required)";
            }
            o << "\n    " << f.help << "\n";
        }
    }
    return o.str();
}

CommandManager::CommandManager() : commandTree("root") {}

CommandManager::~CommandManager() {}

bool CommandManager::registerCommand(vector<std::string> commandPath,
                                     CommandCallback callback,
                                     const string& commandHelp,
                                     const std::vector<CommandFlagDef>& flagDefs)
{
    std::reverse(commandPath.begin(), commandPath.end());
    return commandTree.registerCommand(commandPath, callback, commandHelp,
                                       flagDefs);
}

string CommandManager::executeCommand(const std::string& command)
{
    std::istringstream iss(command);
    std::string cmdPathStr;
    iss >> cmdPathStr;
    if (cmdPathStr.empty())
    {
        return "Failed: Empty command";
    }
    std::vector<std::string> commandPath;
    std::istringstream pathStream(cmdPathStr);
    std::string segment;
    while (std::getline(pathStream, segment, '.'))
    {
        if (!segment.empty())
        {
            commandPath.push_back(segment);
        }
    }
    std::reverse(commandPath.begin(), commandPath.end());

    std::vector<std::string> argTokens{std::istream_iterator<std::string>{iss},
                                       std::istream_iterator<std::string>()};
    const CommandArgs args = parseArgumentTokens(argTokens);

    return commandTree.execute(commandPath, args);
}

string CommandManager::help(const std::string& commandPathStr)
{
    if (commandPathStr.empty())
    {
        return commandTree.helpList("");
    }
    std::vector<string> segs;
    std::istringstream ps(commandPathStr);
    std::string s;
    while (std::getline(ps, s, '.'))
    {
        if (!s.empty())
        {
            segs.push_back(s);
        }
    }
    return commandTree.helpNavigate(segs, "");
}

}  // namespace cmd
