#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scheduler {

template <typename T>
class TFuture;

template <typename Arg>
struct ArgTraits {
    using Type = Arg&;

    static Type Resolve(Arg& arg) { return arg; }
};

template <typename T>
struct ArgTraits<TFuture<T>> {
    using Type = T;

    static Type Resolve(TFuture<T>& future) { return future.Get(); }
};

template <typename Arg>
decltype(auto) Resolve(Arg& arg) {
    return ArgTraits<Arg>::Resolve(arg);
}

template <typename F, typename... Args>
using InvokeResult =
    std::invoke_result_t<F&, typename ArgTraits<Args>::Type...>;
template <typename F, typename... Args>
using StoredResult = std::remove_cvref_t<InvokeResult<F, Args...>>;

class AccessAfterMove : public std::exception {
   public:
    const char* what() const noexcept override {
        return "error: access after move";
    }
};

class BadTaskType : public std::exception {
   public:
    const char* what() const noexcept override {
        return "error: bad task result type";
    }
};

class INode {
   public:
    virtual void Execute() = 0;
    virtual ~INode() noexcept = default;
    virtual bool IsExecuted() const noexcept = 0;
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
        if (expired_) {
            throw AccessAfterMove{};
        }
        result_ = std::make_unique<StoredResultT>(
            Call(std::index_sequence_for<Args...>{}));
        executed_ = true;
    }
    bool IsExecuted() const noexcept override { return executed_; }

    StoredResultT& GetResultRef() override {
        if (expired_) {
            throw AccessAfterMove{};
        }
        return *result_;
    }

    StoredResultT&& MoveResult() override {
        if (expired_) {
            throw AccessAfterMove{};
        }
        expired_ = true;
        return std::move(*result_);
    }

   private:
    StoredF func_;
    StoredArgs args_;
    std::unique_ptr<StoredResultT> result_{};
    bool executed_ = false;
    bool expired_ = false;

    template <std::size_t... Is>
    ResultT Call(std::index_sequence<Is...>) {
        return func_(Resolve(std::get<Is>(args_))...);
    }
};

template <typename T>
class TFuture {
   public:
    explicit TFuture(std::shared_ptr<INode> node) : node_(std::move(node)) {}

    T Get() {
        static_assert(!std::is_rvalue_reference_v<T>);
        static_assert(!std::is_lvalue_reference_v<T> ||
                      std::is_const_v<std::remove_reference_t<T>>);

        using StoredT = std::remove_cvref_t<T>;
        node_->Execute();
        auto* typed_node = dynamic_cast<ITypedNode<StoredT>*>(node_.get());
        if (typed_node == nullptr) {
            throw BadTaskType{};
        }
        if constexpr (std::is_lvalue_reference_v<T>) {
            return typed_node->GetResultRef();
        } else {
            return typed_node->MoveResult();
        }
    }

   private:
    std::shared_ptr<INode> node_;
};

class TTaskScheduler;

class TTask {
   public:
    TTask(std::shared_ptr<INode> node, TTaskScheduler* scheduler)
        : node_(std::move(node)), scheduler_(scheduler) {}

    template <typename T>
    T GetResultSync() {
        static_assert(!std::is_rvalue_reference_v<T>);
        static_assert(!std::is_lvalue_reference_v<T> ||
                      std::is_const_v<std::remove_reference_t<T>>);

        using StoredT = std::remove_cvref_t<T>;
        node_->Execute();
        auto* typed_node = dynamic_cast<ITypedNode<StoredT>*>(node_.get());
        if (typed_node == nullptr) {
            throw BadTaskType{};
        }
        return static_cast<T>(typed_node->GetResultRef());
    }

    template <typename T>
    TFuture<T> GetFutureResult() {
        return TFuture<T>(node_);
    }

   private:
    std::shared_ptr<INode> node_;
    TTaskScheduler* scheduler_;
};

class TTaskScheduler {
   private:
    std::vector<std::shared_ptr<INode>> tasks_;

   public:
    template <typename F, typename... Args>
    TTask Add(F&& f, Args&&... args) {
        using NodeT =
            Node<std::remove_cvref_t<F>, std::remove_cvref_t<Args>...>;
        auto node = std::make_shared<NodeT>(std::forward<F>(f),
                                            std::forward<Args>(args)...);

        tasks_.push_back(node);
        return TTask(node, this);
    }

    void ExecuteAll() {
        for (auto& task : tasks_) {
            task->Execute();
        }
    }
};

}  // namespace scheduler
