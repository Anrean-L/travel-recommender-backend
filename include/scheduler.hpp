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
using StoredResult = std::decay_t<InvokeResult<F, Args...>>;

template <typename F>
struct FunctionTraits;

template <typename R, typename Arg>
struct FunctionTraits<R (*)(Arg)> {
    using ArgType = Arg;
};

template <typename R, typename C, typename Arg>
struct FunctionTraits<R (C::*)(Arg) const> {
    using ArgType = Arg;
};

template <typename F>
struct FunctionTraits {
    using ArgType = typename FunctionTraits<decltype(&F::operator())>::ArgType;
};

template <typename R, typename C, typename Arg>
struct FunctionTraits<R (C::*)(Arg)> {
    using ArgType = Arg;
};

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

class CycleDetected : public std::exception {
   public:
    const char* what() const noexcept override {
        return "error: cycle detected";
    }
};

class INode {
   public:
    virtual void Execute() = 0;
    virtual ~INode() noexcept = default;

   protected:
    enum class State {
        kNotStarted,
        kExecuting,
        kExecuted,
    };
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
    using StoredF = std::decay_t<F>;
    using StoredArgs = std::tuple<std::decay_t<Args>...>;

   public:
    Node(F f, Args... args) : func_(std::move(f)), args_(std::move(args)...) {}
    void Execute() override {
        if (state_ == INode::State::kExecuting) {
            throw CycleDetected{};
        }
        if (state_ == INode::State::kExecuted) {
            return;
        }
        state_ = INode::State::kExecuting;
        if (expired_) {
            throw AccessAfterMove{};
        }
        if constexpr (std::is_member_function_pointer_v<StoredF>) {
            result_ = std::make_unique<StoredResultT>(
                CallMethod(std::make_index_sequence<sizeof...(Args) - 1>{}));
        } else {
            result_ = std::make_unique<StoredResultT>(
                CallRegular(std::index_sequence_for<Args...>{}));
        }
        state_ = INode::State::kExecuted;
    }

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
    INode::State state_ = INode::State::kNotStarted;
    bool expired_ = false;

    template <std::size_t... Is>
    ResultT CallMethod(std::index_sequence<Is...>) {
        auto&& obj = Resolve(std::get<0>(args_));
        if constexpr (std::is_pointer_v<
                          std::remove_reference_t<decltype(obj)>>) {
            return ((*obj).*func_)(Resolve(std::get<Is + 1>(args_))...);
        } else {
            return (obj.*func_)(Resolve(std::get<Is + 1>(args_))...);
        }
    }

    template <std::size_t... Is>
    ResultT CallRegular(std::index_sequence<Is...>) {
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

        using StoredT = std::decay_t<T>;
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

        using StoredT = std::decay_t<T>;
        node_->Execute();
        auto* typed_node = dynamic_cast<ITypedNode<StoredT>*>(node_.get());
        if (typed_node == nullptr) {
            throw BadTaskType{};
        }
        return static_cast<T>(typed_node->GetResultRef());
    }

    template <typename F>
    TTask Apply(F&& f);

    template <typename Method, typename Obj>
    TTask Apply(Method&& f, Obj&& obj);

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
        using NodeT = Node<std::decay_t<F>, std::decay_t<Args>...>;
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

template <typename F>
TTask TTask::Apply(F&& f) {
    using Arg = typename FunctionTraits<std::decay_t<F>>::ArgType;
    return scheduler_->Add(std::forward<F>(f), GetFutureResult<Arg>());
}

template <typename Method, typename Obj>
TTask TTask::Apply(Method&& method, Obj&& obj) {
    using Arg = typename FunctionTraits<std::decay_t<Method>>::ArgType;
    return scheduler_->Add(std::forward<Method>(method), std::forward<Obj>(obj),
                           GetFutureResult<Arg>());
}

}  // namespace scheduler
