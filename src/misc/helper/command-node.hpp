#ifndef COMMAND_NODE_HPP
#define COMMAND_NODE_HPP

#include <std-inc.hpp>

namespace cmd
{

typedef std::function<string(const std::vector<std::string>& arguments)>
    CommandCallback;

class CommandNode
{
  public:
    CommandNode(const string& key);
    virtual ~CommandNode();
    virtual bool registerCommand(vector<std::string> commandPath,
                                 CommandCallback callback,
                                 uint8_t minArgCnt = 0) = 0;
    virtual string execute(vector<std::string> commandPath,
                           const std::vector<std::string>& arguments) = 0;
    const string& getKeyword() const;
    virtual bool isLeaf() const = 0;
    virtual string help(const string& command) = 0;

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
                         uint8_t minArgCnt = 0) override;
    string execute(vector<std::string> commandPath,
                   const std::vector<std::string>& arguments) override;
    auto findChildBranch(const string& keyword);
    auto findChildLeaf(const string& keyword);
    bool isLeaf() const override
    {
        return false;
    }
    string help(const string& command) override;

  private:
    std::vector<std::shared_ptr<CommandNode>> children;
};

class CommandLeaf : public CommandNode
{
  public:
    CommandLeaf(const string& key,
                CommandCallback callback,
                uint8_t minArgCnt = 0);
    ~CommandLeaf();
    bool registerCommand(vector<std::string> commandPath,
                         CommandCallback callback,
                         uint8_t minArgCnt = 0) override;
    string execute(vector<std::string> commandPath,
                   const std::vector<std::string>& arguments) override;
    bool isLeaf() const override
    {
        return true;
    }
    string help(const string& command) override;

  private:
    std::string value;
    CommandCallback callback;
    uint8_t minArgCnt;
};

class CommandManager
{
  public:
    CommandManager();
    ~CommandManager();
    bool registerCommand(vector<std::string> commandPath,
                         CommandCallback callback,
                         uint8_t minArgCnt = 0);
    string executeCommand(const std::string& command);
    string help(const std::string& command);

  private:
    CommandBranch commandTree;
};

}  // namespace cmd

#endif