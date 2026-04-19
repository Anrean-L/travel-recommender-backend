#pragma once

#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scheduler {

template <typename F, typename... Args>
using InvokeResult = std::invoke_result_t<F&, Args&...>;
template <typename F, typename... Args>
using StoredResult = std::remove_cvref_t<InvokeResult<F, Args...>>;

class INode {
   public:
    virtual void Execute() = 0;
    virtual ~INode() = default;
    virtual bool IsExecuted() const = 0;
};

template <typename R>
class ITypedNode : public INode {
   public:
    virtual R& GetResultRef() = 0;
    virtual R&& MoveResult() = 0;
};

template <typename F, typename... Args>
class Node : public ITypedNode<StoredResult<F, Args...>> {
   private:
    using ResultT = InvokeResult<F, Args...>;
    using StoredResultT = StoredResult<F, Args...>;
    using StoredF = std::remove_cvref_t<F>;
    using StoredArgs = std::tuple<std::decay_t<Args>...>;

   public:
    Node(F f, Args... args) : func_(std::move(f)), args_(std::move(args)...) {}
    void Execute() override {
        if (executed_) {
            return;
        }
        result_ = std::make_unique<StoredResultT>(
            Call(std::index_sequence_for<Args...>{}));
        executed_ = true;
    }
    bool IsExecuted() const override { return executed_; }

    StoredResultT& GetResultRef() override { return *result_; }

    StoredResultT&& MoveResult() override { return std::move(*result_); }

   private:
    StoredF func_;
    StoredArgs args_;
    std::unique_ptr<StoredResultT> result_{};
    bool executed_ = false;

    template <size_t... Is>
    ResultT Call(std::index_sequence<Is...>) {
        return func_(std::get<Is>(args_)...);
    }
};

class TTaskScheduler;

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

}  // namespace scheduler
