#pragma once

#include <cstddef>
#include <utility>

namespace scheduler::meta {

template <std::size_t... Is>
struct IndexSequence {};

template <std::size_t N, std::size_t... Is>
struct MakeIndexSequenceImpl : MakeIndexSequenceImpl<N - 1, N - 1, Is...> {};

template <std::size_t... Is>
struct MakeIndexSequenceImpl<0, Is...> {
    using Type = IndexSequence<Is...>;
};

template <std::size_t N>
using MakeIndexSequence = typename MakeIndexSequenceImpl<N>::Type;

template <typename... Ts>
using IndexSequenceFor = MakeIndexSequence<sizeof...(Ts)>;

template <std::size_t I, typename... Ts>
struct TypeAt;

template <typename T, typename... Ts>
struct TypeAt<0, T, Ts...> {
    using Type = T;
};

template <std::size_t I, typename T, typename... Ts>
struct TypeAt<I, T, Ts...> : TypeAt<I - 1, Ts...> {};

template <std::size_t I, typename... Ts>
using TypeAtT = typename TypeAt<I, Ts...>::Type;

template <typename... Ts>
struct TypeList {
    static constexpr std::size_t kSize = sizeof...(Ts);

    template <std::size_t I>
    using At = TypeAt<I, Ts...>;
};

template <std::size_t I, typename T>
struct TupleItem {
    T value;

    TupleItem() = default;

    template <typename U>
    explicit TupleItem(U&& value) : value(std::forward<U>(value)) {}
};

template <typename Seq, typename... Is>
class TupleImpl;

template <std::size_t... Is, typename... Ts>
class TupleImpl<IndexSequence<Is...>, Ts...> : private TupleItem<Is, Ts>... {
   public:
    TupleImpl() = default;

    template <typename... Us>
    explicit TupleImpl(Us&&... values)
        : TupleItem<Is, Ts>(std::forward<Us>(values))... {}

    template <std::size_t I>
    auto& Get() & {
        using T = TypeAtT<I, Ts...>;
        return static_cast<TupleItem<I, T>&>(*this).value;
    }

    template <std::size_t I>
    const auto& Get() const& {
        using T = TypeAtT<I, Ts...>;
        return static_cast<const TupleItem<I, T>&>(*this).value;
    }

    template <std::size_t I>
    auto&& Get() && {
        using T = TypeAtT<I, Ts...>;
        return std::move(static_cast<TupleItem<I, T>&>(*this).value);
    }
};

template <typename... Ts>
class Tuple : public TupleImpl<IndexSequenceFor<Ts...>, Ts...> {
   private:
    using Base = TupleImpl<IndexSequenceFor<Ts...>, Ts...>;

   public:
    using Base::Base;

    static constexpr std::size_t kSize = sizeof...(Ts);

    template <std::size_t I>
    using Element = TypeAtT<I, Ts...>;
};

template <std::size_t I, typename... Ts>
decltype(auto) Get(Tuple<Ts...>& tuple) {
    return tuple.template Get<I>();
}

template <std::size_t I, typename... Ts>
decltype(auto) Get(const Tuple<Ts...>& tuple) {
    return tuple.template Get<I>();
}

template <std::size_t I, typename... Ts>
decltype(auto) Get(Tuple<Ts...>&& tuple) {
    return std::move(tuple).template Get<I>();
}

template <std::size_t I, typename List>
struct ListElement;

template <std::size_t I, typename... Ts>
struct ListElement<I, TypeList<Ts...>> {
    using Type = TypeAtT<I, Ts...>;
};

template <std::size_t I, typename List>
using ListElementT = typename ListElement<I, List>::Type;

template <typename T>
T&& Declval() noexcept;

template <typename F, typename... Args>
auto Invoke(F&& f, Args&&... args)
    -> decltype(static_cast<F&&>(f)(static_cast<Args&&>(args)...));

template <typename Method, typename Obj, typename... Args>
auto Invoke(Method method, Obj&& obj, Args&&... args)
    -> decltype((static_cast<Obj&&>(obj).*
                 method)(static_cast<Args&&>(args)...));

template <typename Method, typename ObjPtr, typename... Args>
auto Invoke(Method method, ObjPtr&& obj, Args&&... args)
    -> decltype(((*static_cast<ObjPtr&&>(obj)).*
                 method)(static_cast<Args&&>(args)...));

template <typename F, typename... Args>
struct InvokeResult {
    using Type = decltype(Invoke(Declval<F>(), Declval<Args>()...));
};

template <typename F, typename... Args>
using InvokeResultT = typename InvokeResult<F, Args...>::Type;

}  // namespace scheduler::meta
