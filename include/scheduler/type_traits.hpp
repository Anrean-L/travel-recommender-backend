#pragma once

namespace scheduler::meta {

template <typename T>
struct RemoveReference {
    using Type = T;
};

template <typename T>
struct RemoveReference<T&> {
    using Type = T;
};

template <typename T>
struct RemoveReference<T&&> {
    using Type = T;
};

template <typename T>
using RemoveReferenceT = typename RemoveReference<T>::Type;

template <typename T>
struct RemoveConst {
    using Type = T;
};

template <typename T>
struct RemoveConst<const T> {
    using Type = T;
};

template <typename T>
using RemoveConstT = RemoveConst<T>::Type;

template <typename T>
struct RemoveVolatile {
    using Type = T;
};

template <typename T>
struct RemoveVolatile<volatile T> {
    using Type = T;
};

template <typename T>
using RemoveVolatileT = RemoveVolatile<T>::Type;

template <typename T>
using RemoveCvT = RemoveVolatileT<RemoveConstT<T>>;

template <typename T>
using RemoveCvRefT = RemoveCvT<RemoveReferenceT<T>>;

template <typename T>
struct IsPointer {
    static constexpr bool kValue = false;
};
template <typename T>
struct IsPointer<T*> {
    static constexpr bool kValue = true;
};
template <typename T>
struct IsPointer<T* const> {
    static constexpr bool kValue = true;
};
template <typename T>
struct IsPointer<T* volatile> {
    static constexpr bool kValue = true;
};
template <typename T>
struct IsPointer<T* const volatile> {
    static constexpr bool kValue = true;
};

template <typename T>
inline constexpr bool IsPointerV = IsPointer<T>::kValue;

template <typename T>
struct IsLvalueReference {
    static constexpr bool kValue = false;
};

template <typename T>
struct IsLvalueReference<T&> {
    static constexpr bool kValue = true;
};

template <typename T>
inline constexpr bool IsLvalueReferenceV = IsLvalueReference<T>::kValue;

template <typename T>
struct IsRvalueReference {
    static constexpr bool kValue = false;
};

template <typename T>
struct IsRvalueReference<T&&> {
    static constexpr bool kValue = true;
};

template <typename T>
inline constexpr bool IsRvalueReferenceV = IsRvalueReference<T>::kValue;

template <typename T>
struct IsConst {
    static constexpr bool kValue = false;
};

template <typename T>
struct IsConst<const T> {
    static constexpr bool kValue = true;
};

template <typename T>
inline constexpr bool IsConstV = IsConst<T>::kValue;

template <typename T>
struct AddPointer {
    using Type = T*;
};

template <typename T>
using AddPointerT = AddPointer<T>::Type;

template <typename T>
struct IsFunction {
    static constexpr bool kValue = false;
};

template <typename R, typename... Args>
struct IsFunction<R(Args...)> {
    static constexpr bool kValue = true;
};

template <typename R, typename... Args>
struct IsFunction<R(Args..., ...)> {
    static constexpr bool kValue = true;
};

template <typename T>
inline constexpr bool IsFunctionV = IsFunction<T>::kValue;

template <bool C, typename T, typename F>
struct Conditional {
    using Type = T;
};

template <typename T, typename F>
struct Conditional<false, T, F> {
    using Type = F;
};

template <bool C, typename T, typename F>
using ConditionalT = typename Conditional<C, T, F>::Type;

template <typename T>
struct Decay {
   private:
    using U = RemoveReferenceT<T>;

   public:
    using Type = ConditionalT<IsFunctionV<U>, AddPointerT<U>, RemoveCvT<U>>;
};

template <typename T>
using DecayT = Decay<T>::Type;

template <typename T>
struct IsMemberFunctionPointer {
    static constexpr bool kValue = false;
};

template <typename R, typename C, typename... Args>
struct IsMemberFunctionPointer<R (C::*)(Args...)> {
    static constexpr bool kValue = true;
};
template <typename R, typename C, typename... Args>
struct IsMemberFunctionPointer<R (C::*)(Args...) const> {
    static constexpr bool kValue = true;
};
template <typename R, typename C, typename... Args>
struct IsMemberFunctionPointer<R (C::*)(Args...) volatile> {
    static constexpr bool kValue = true;
};
template <typename R, typename C, typename... Args>
struct IsMemberFunctionPointer<R (C::*)(Args...) const volatile> {
    static constexpr bool kValue = true;
};

template <typename T>
inline constexpr bool IsMemberFunctionPointerV =
    IsMemberFunctionPointer<T>::kValue;

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