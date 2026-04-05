#ifndef COMMAND_NODE_HPP
#define COMMAND_NODE_HPP

#include <std-inc.hpp>
#include <unordered_map>

namespace cmd
{

struct CommandArgs
{
    std::unordered_map<string, string> flags;
    std::vector<string> positionals;
};

struct CommandFlagDef
{
    string name;
    string help;
    bool required = false;
};

typedef std::function<string(const CommandArgs&)> CommandCallback;

class CommandNode
{
  public:
    CommandNode(const string& key);
    virtual ~CommandNode();
    virtual bool registerCommand(vector<std::string> commandPath,
                                 CommandCallback callback,
                                 const string& commandHelp,
                                 const std::vector<CommandFlagDef>& flagDefs) = 0;
    virtual string execute(vector<std::string> commandPath,
                           const CommandArgs& args) = 0;
    const string& getKeyword() const;
    virtual bool isLeaf() const = 0;
    virtual string helpSummary(const string& dottedPath) const = 0;

  protected:
    std::string keyword;
};

class CommandBranch : public CommandNode
{
  public:
    CommandBranch(const string& key);
    ~CommandBranch();
    bool registerCommand(vector<std::string> commandPath,
                         CommandCallback callback,
                         const string& commandHelp,
                         const std::vector<CommandFlagDef>& flagDefs) override;
    string execute(vector<std::string> commandPath,
                   const CommandArgs& args) override;
    auto findChildBranch(const string& kw) const;
    auto findChildLeaf(const string& kw) const;
    bool isLeaf() const override
    {
        return false;
    }
    string helpSummary(const string& dottedPath) const override;
    string helpList(const string& prefix) const;
    string helpNavigate(const std::vector<string>& path,
                        const string& prefix) const;

  private:
    std::vector<std::shared_ptr<CommandNode>> children;
};

class CommandLeaf : public CommandNode
{
  public:
    CommandLeaf(const string& key,
                CommandCallback callback,
                string commandHelp,
                std::vector<CommandFlagDef> flagDefs);
    ~CommandLeaf();
    bool registerCommand(vector<std::string> commandPath,
                         CommandCallback callback,
                         const string& commandHelp,
                         const std::vector<CommandFlagDef>& flagDefs) override;
    string execute(vector<std::string> commandPath,
                   const CommandArgs& args) override;
    bool isLeaf() const override
    {
        return true;
    }
    string helpSummary(const string& dottedPath) const override;
    string formatFullHelp(const string& dottedPath) const;

  private:
    CommandCallback callback;
    string commandHelp;
    std::vector<CommandFlagDef> flagDefs;
};

class CommandManager
{
  public:
    CommandManager();
    ~CommandManager();
    bool registerCommand(vector<std::string> commandPath,
                         CommandCallback callback,
                         const string& commandHelp = {},
                         const std::vector<CommandFlagDef>& flagDefs = {});
    string executeCommand(const std::string& command);
    string help(const std::string& commandPath);

    static CommandArgs parseArgumentTokens(const std::vector<string>& tokens);

  private:
    CommandBranch commandTree;
};

}  // namespace cmd

#endif
