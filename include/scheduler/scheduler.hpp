#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

#include "scheduler/tuple.hpp"
#include "scheduler/type_traits.hpp"

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
using InvokeResult = meta::InvokeResultT<F&, typename ArgTraits<Args>::Type...>;
template <typename F, typename... Args>
using StoredResult = meta::DecayT<InvokeResult<F, Args...>>;

template <typename F>
struct FunctionTraits;

template <typename R, typename... Args>
struct FunctionTraits<R (*)(Args...)> {
    using ArgsTuple = meta::TypeList<Args...>;
    static constexpr std::size_t kArity = sizeof...(Args);

    template <std::size_t I>
    using Arg = meta::ListElementT<I, ArgsTuple>;
};

template <typename R, typename C, typename... Args>
struct FunctionTraits<R (C::*)(Args...) const> {
    using ArgsTuple = meta::TypeList<Args...>;
    static constexpr std::size_t kArity = sizeof...(Args);

    template <std::size_t I>
    using Arg = meta::ListElementT<I, ArgsTuple>;
};

template <typename R, typename C, typename... Args>
struct FunctionTraits<R (C::*)(Args...)> {
    using ArgsTuple = meta::TypeList<Args...>;
    static constexpr std::size_t kArity = sizeof...(Args);

    template <std::size_t I>
    using Arg = meta::ListElementT<I, ArgsTuple>;
};

template <typename F>
struct FunctionTraits {
    using Base = FunctionTraits<decltype(&F::operator())>;
    using ArgsTuple = typename Base::ArgsTuple;
    static constexpr std::size_t kArity = Base::kArity;

    template <std::size_t I>
    using Arg = typename Base::template Arg<I>;
};

template <typename Tuple>
struct DecayTuple;

template <typename... Ts>
struct DecayTuple<meta::TypeList<Ts...>> {
    using Type = meta::Tuple<meta::DecayT<Ts>...>;
};

template <typename F, typename Tuple, std::size_t... Is>
decltype(auto) CallFromTupleImpl(F&& f, Tuple&& tuple,
                                 meta::IndexSequence<Is...>) {
    return std::forward<F>(f)(meta::Get<Is>(std::forward<Tuple>(tuple))...);
}

template <typename F, typename Tuple>
decltype(auto) CallFromTuple(F&& f, Tuple&& tuple) {
    constexpr std::size_t N = meta::RemoveReferenceT<Tuple>::kSize;
    return CallFromTupleImpl(std::forward<F>(f), std::forward<Tuple>(tuple),
                             meta::MakeIndexSequence<N>{});
}
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
    using StoredF = meta::DecayT<F>;
    using StoredArgs = meta::Tuple<meta::DecayT<Args>...>;

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
        try {
            if constexpr (meta::IsMemberFunctionPointerV<StoredF>) {
                result_ = std::make_unique<StoredResultT>(
                    CallMethod(meta::MakeIndexSequence<sizeof...(Args) - 1>{}));
            } else {
                result_ = std::make_unique<StoredResultT>(
                    CallRegular(meta::IndexSequenceFor<Args...>{}));
            }
            state_ = INode::State::kExecuted;
        } catch (...) {
            state_ = INode::State::kNotStarted;
            throw;
        }
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
    ResultT CallMethod(meta::IndexSequence<Is...>) {
        auto&& obj = Resolve(meta::Get<0>(args_));
        if constexpr (meta::IsPointerV<meta::RemoveReferenceT<decltype(obj)>>) {
            return ((*obj).*func_)(Resolve(meta::Get<Is + 1>(args_))...);
        } else {
            return (obj.*func_)(Resolve(meta::Get<Is + 1>(args_))...);
        }
    }

    template <std::size_t... Is>
    ResultT CallRegular(meta::IndexSequence<Is...>) {
        return func_(Resolve(meta::Get<Is>(args_))...);
    }
};

template <typename T>
class TFuture {
   public:
    explicit TFuture(std::shared_ptr<INode> node) : node_(std::move(node)) {}

    T Get() {
        static_assert(!meta::IsRvalueReferenceV<T>);
        static_assert(!meta::IsLvalueReferenceV<T> ||
                      meta::IsConstV<meta::RemoveReferenceT<T>>);

        using StoredT = meta::DecayT<T>;
        node_->Execute();
        auto* typed_node = dynamic_cast<ITypedNode<StoredT>*>(node_.get());
        if (typed_node == nullptr) {
            throw BadTaskType{};
        }
        if constexpr (meta::IsLvalueReferenceV<T>) {
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
        static_assert(!meta::IsRvalueReferenceV<T>);
        static_assert(!meta::IsLvalueReferenceV<T> ||
                      meta::IsConstV<meta::RemoveReferenceT<T>>);

        using StoredT = meta::DecayT<T>;
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
        using NodeT = Node<meta::DecayT<F>, meta::DecayT<Args>...>;
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
    using Traits = FunctionTraits<meta::DecayT<F>>;
    if constexpr (Traits::kArity == 1) {
        using Arg = typename Traits::template Arg<0>;
        return scheduler_->Add(std::forward<F>(f), GetFutureResult<Arg>());
    } else {
        using ArgsTuple = typename Traits::ArgsTuple;
        using SourceTuple = typename DecayTuple<ArgsTuple>::Type;

        auto wrapper = [func = std::forward<F>(f)](SourceTuple tuple) mutable {
            return CallFromTuple(func, std::move(tuple));
        };
        return scheduler_->Add(std::move(wrapper),
                               GetFutureResult<SourceTuple>());
    }
}

template <typename Method, typename Obj>
TTask TTask::Apply(Method&& method, Obj&& obj) {
    using Arg = typename FunctionTraits<meta::DecayT<Method>>::template Arg<0>;
    return scheduler_->Add(std::forward<Method>(method), std::forward<Obj>(obj),
                           GetFutureResult<Arg>());
}

}  // namespace scheduler
