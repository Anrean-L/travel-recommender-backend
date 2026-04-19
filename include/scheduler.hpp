#pragma once

#include <memory>
#include <vector>

class INode {
   public:
    virtual void Execute() = 0;
    virtual ~INode() = default;
    virtual bool IsExecuted() const = 0;
};

template <typename F, typename... Args>
class Node : public INode {};

class TTask {
   public:
    explicit TTask(std::shared_ptr<INode> node) : node_(node) {}

   private:
    std::shared_ptr<INode> node_;
};

class TTaskScheduler {
   private:
    std::vector<std::shared_ptr<INode>> tasks_;

   public:
    template <typename F, typename... Args>
    TTask add(F&& f, Args&&... args);

    void ExecuteAll() {
        for (auto& task : tasks_) {
            task->Execute();
        }
    }
};
