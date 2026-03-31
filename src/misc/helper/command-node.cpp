#include "command-node.hpp"

namespace cmd
{

CommandNode::CommandNode(const string& key) : keyword(key) {}

CommandNode::~CommandNode() {}

const string& CommandNode::getKeyword() const
{
    return keyword;
}

CommandBranch::CommandBranch(const string& key) : CommandNode(key) {}

CommandBranch::~CommandBranch() {}

auto CommandBranch::findChildBranch(const string& keyword)
{
    return std::find_if(
        children.begin(),
        children.end(),
        [&keyword](const std::shared_ptr<CommandNode>& node)
        { return node->getKeyword() == keyword && !node->isLeaf(); });
}

auto CommandBranch::findChildLeaf(const string& keyword)
{
    return std::find_if(
        children.begin(),
        children.end(),
        [&keyword](const std::shared_ptr<CommandNode>& node)
        { return node->getKeyword() == keyword && node->isLeaf(); });
}

bool CommandBranch::registerCommand(vector<std::string> commandPath,
                                    CommandCallback callback,
                                    uint8_t minArgCnt)
{
    const std::string next = commandPath.back();
    commandPath.pop_back();
    auto it = findChildLeaf(next);
    if (commandPath.empty())
    {
        auto it = findChildLeaf(next);
        if (it != children.end())
        {
            return (*it)->registerCommand(commandPath, callback, minArgCnt);
        }
        else
        {
            children.push_back(
                std::make_shared<CommandLeaf>(next, callback, minArgCnt));
            return true;
        }
    }
    else
    {
        auto it = findChildBranch(next);
        if (it != children.end())
        {
            // Next already exists
            return (*it)->registerCommand(commandPath, callback, minArgCnt);
        }
        else
        {
            // Next doesn't exist yet
            if (commandPath.empty())
            {
                children.push_back(
                    std::make_shared<CommandLeaf>(next, callback, minArgCnt));
                return true;
            }
            else
            {
                children.push_back(std::make_shared<CommandBranch>(next));
                return children.back()->registerCommand(
                    commandPath, callback, minArgCnt);
            }
        }
    }
}

string CommandBranch::execute(vector<std::string> commandPath,
                              const std::vector<std::string>& arguments)
{
    LG_D("Visiting command branch {}, command path: {}",
         keyword.c_str(),
         commandPath);
    if (commandPath.empty())
    {
        return "Failed: Invalid command path";
    }
    else
    {
        const std::string next = commandPath.back();
        commandPath.pop_back();
        auto it = children.end();
        if (commandPath.empty())
        {
            it = findChildLeaf(next);
        }
        else
        {
            it = findChildBranch(next);
        }
        if (it != children.end())
        {
            return (*it)->execute(commandPath, arguments);
        }
        else
        {
            return "Failed: Command not found";
        }
    }
}

string CommandBranch::help(const string& command)
{
    string helpString;
    for(const auto& child : children)
    {
        string cmd;
        if(command.empty())
        {
            cmd = child->getKeyword();
        }
        else
        {
            cmd = command + "." + child->getKeyword();
        }
        helpString += child->help(cmd) + "\n";
    }
    return helpString;
}

bool CommandLeaf::registerCommand(vector<std::string> commandPath,
                                  CommandCallback callback,
                                  uint8_t minArgCnt)
{
    LG_W("Command exists already as leaf. Overwriting command.");
    callback = callback;
    minArgCnt = minArgCnt;
    return true;
}

CommandLeaf::CommandLeaf(const string& key,
                         CommandCallback callback,
                         uint8_t minArgCnt)
    : CommandNode(key), callback(callback), minArgCnt(minArgCnt)
{
}

CommandLeaf::~CommandLeaf() {}

string CommandLeaf::execute(vector<std::string> commandPath,
                            const std::vector<std::string>& arguments)
{
    LG_D("Visiting command leaf {}, command path: {}",
         keyword.c_str(),
         commandPath);
    if (!commandPath.empty())
    {
        return "Failed: Invalid command path";
    }
    if (arguments.size() < minArgCnt)
    {
        return "Failed: Not enough arguments";
    }
    return callback(arguments);
}

string CommandLeaf::help(const string& command)
{
    return command + ": Help";
}

CommandManager::CommandManager() : commandTree("root") {}
CommandManager::~CommandManager() {}
bool CommandManager::registerCommand(vector<std::string> commandPath,
                                     CommandCallback callback,
                                     uint8_t minArgCnt)
{
    std::reverse(commandPath.begin(), commandPath.end());
    return commandTree.registerCommand(commandPath, callback, minArgCnt);
}
string CommandManager::executeCommand(const std::string& command)
{
    std::istringstream iss(command);
    std::string cmdPathStr;
    iss >> cmdPathStr;
    std::vector<std::string> commandPath;
    std::istringstream pathStream(cmdPathStr);
    std::string segment;
    while (std::getline(pathStream, segment, '.'))
    {
        commandPath.push_back(segment);
    }
    std::reverse(commandPath.begin(), commandPath.end());

    std::vector<std::string> arguments{std::istream_iterator<std::string>{iss},
                                       std::istream_iterator<std::string>()};

    return commandTree.execute(commandPath, arguments);
}

string CommandManager::help(const std::string& command)
{
    return commandTree.help("");
}

}  // namespace cmd
