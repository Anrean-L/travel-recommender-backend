#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "scheduler/scheduler.hpp"

namespace {

int AddInts(int lhs, int rhs) { return lhs + rhs; }

int Increment(int value) { return value + 1; }

int SumThree(int lhs, int rhs, int extra) { return lhs + rhs + extra; }

struct Multiplier {
    int factor = 1;

    int Multiply(int value) const { return value * factor; }

    int AddToFactor(int value) { return value + factor; }
};

struct Identity {
    int operator()(int value) const { return value; }
};

}  // namespace

TEST(SchedulerTraits, ResolveReturnsPlainArgumentByReference) {
    int value = 7;

    decltype(auto) resolved = scheduler::Resolve(value);

    static_assert(std::is_same_v<decltype(resolved), int&>);
    EXPECT_EQ(&resolved, &value);
    resolved = 9;
    EXPECT_EQ(value, 9);
}

TEST(SchedulerTraits, FunctionTraitsDetectCallableArgumentTypes) {
    auto lambda = [](const int& value, int multiplier) {
        return value * multiplier;
    };

    static_assert(scheduler::FunctionTraits<decltype(&Increment)>::kArity == 1);
    static_assert(
        std::is_same_v<
            scheduler::FunctionTraits<decltype(&Increment)>::template Arg<0>,
            int>);
    static_assert(scheduler::FunctionTraits<Identity>::kArity == 1);
    static_assert(
        std::is_same_v<scheduler::FunctionTraits<Identity>::template Arg<0>,
                       int>);
    static_assert(scheduler::FunctionTraits<decltype(lambda)>::kArity == 2);
    static_assert(std::is_same_v<
                  scheduler::FunctionTraits<decltype(lambda)>::template Arg<0>,
                  const int&>);
    static_assert(
        std::is_same_v<
            scheduler::FunctionTraits<decltype(lambda)>::template Arg<1>, int>);
    static_assert(
        scheduler::FunctionTraits<decltype(&Multiplier::Multiply)>::kArity ==
        1);
    static_assert(
        std::is_same_v<scheduler::FunctionTraits<
                           decltype(&Multiplier::Multiply)>::template Arg<0>,
                       int>);
}

TEST(SchedulerExceptions, WhatMessagesAreStable) {
    EXPECT_STREQ(scheduler::AccessAfterMove{}.what(),
                 "error: access after move");
    EXPECT_STREQ(scheduler::BadTaskType{}.what(),
                 "error: bad task result type");
    EXPECT_STREQ(scheduler::CycleDetected{}.what(), "error: cycle detected");
}

TEST(TaskScheduler, AddExecutesLambdaWithoutArguments) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return 42; });

    EXPECT_EQ(task.GetResultSync<int>(), 42);
}

TEST(TaskScheduler, AddPassesStoredArgumentsToCallable) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add(AddInts, 20, 22);

    EXPECT_EQ(task.GetResultSync<int>(), 42);
}

TEST(TaskScheduler, AddSupportsFunctorObjects) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add(Identity{}, 42);

    EXPECT_EQ(task.GetResultSync<int>(), 42);
}

TEST(TaskScheduler, AddSupportsMemberFunctionWithObjectValue) {
    scheduler::TTaskScheduler scheduler;
    Multiplier multiplier{3};
    auto task = scheduler.Add(&Multiplier::Multiply, multiplier, 14);

    EXPECT_EQ(task.GetResultSync<int>(), 42);
}

TEST(TaskScheduler, AddSupportsMemberFunctionWithObjectPointer) {
    scheduler::TTaskScheduler scheduler;
    Multiplier multiplier{10};
    auto task = scheduler.Add(&Multiplier::AddToFactor, &multiplier, 32);

    EXPECT_EQ(task.GetResultSync<int>(), 42);
}

TEST(TaskScheduler, ExecuteAllRunsEachTaskOnce) {
    scheduler::TTaskScheduler scheduler;
    int calls = 0;
    auto first = scheduler.Add([&calls] {
        ++calls;
        return 20;
    });
    auto second = scheduler.Add([](const int& value) { return value + 22; },
                                first.GetFutureResult<const int&>());

    scheduler.ExecuteAll();
    scheduler.ExecuteAll();

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(first.GetResultSync<int>(), 20);
    EXPECT_EQ(second.GetResultSync<int>(), 42);
}

TEST(Task, GetResultSyncCanBeCalledRepeatedly) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return std::string{"answer"}; });

    EXPECT_EQ(task.GetResultSync<std::string>(), "answer");
    EXPECT_EQ(task.GetResultSync<const std::string&>(), "answer");
}

TEST(Task, GetResultSyncThrowsForWrongType) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return 42; });

    EXPECT_THROW(task.GetResultSync<double>(), scheduler::BadTaskType);
}

TEST(Task, GetFutureResultReturnsReferenceFutureWithoutMovingResult) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return std::vector<int>{1, 2, 3}; });
    auto future = task.GetFutureResult<const std::vector<int>&>();

    const auto& first = future.Get();
    const auto& second = future.Get();

    EXPECT_EQ(first, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(&first, &second);
    EXPECT_EQ(task.GetResultSync<const std::vector<int>&>().size(), 3);
}

TEST(Task, GetFutureResultReturnsMovableFuture) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return std::string{"answer"}; });
    auto future = task.GetFutureResult<std::string>();

    EXPECT_EQ(future.Get(), "answer");
    EXPECT_THROW(future.Get(), scheduler::AccessAfterMove);
    EXPECT_THROW(task.GetResultSync<std::string>(), scheduler::AccessAfterMove);
}

TEST(Task, FutureGetThrowsForWrongType) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return 42; });
    auto future = task.GetFutureResult<double>();

    EXPECT_THROW(future.Get(), scheduler::BadTaskType);
}

TEST(TaskScheduler, FutureCanBeUsedAsRegularTaskArgument) {
    scheduler::TTaskScheduler scheduler;
    auto first = scheduler.Add([] { return 21; });
    auto second = scheduler.Add([](const int& value) { return value * 2; },
                                first.GetFutureResult<const int&>());

    EXPECT_EQ(second.GetResultSync<int>(), 42);
}

TEST(Task, ApplyCreatesDependentTaskFromFunctionPointer) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return 41; });
    auto next = task.Apply(&Increment);

    EXPECT_EQ(next.GetResultSync<int>(), 42);
}

TEST(Task, ApplyCreatesDependentTaskFromFunctionName) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return 41; });
    auto next = task.Apply(Increment);

    EXPECT_EQ(next.GetResultSync<int>(), 42);
}

TEST(Task, ApplyCreatesDependentTaskFromLambda) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return 21; });
    auto next = task.Apply([](int value) { return value * 2; });

    EXPECT_EQ(next.GetResultSync<int>(), 42);
}

TEST(Task, ApplyCreatesDependentTaskFromMultiArgumentFunction) {
    scheduler::TTaskScheduler scheduler;
    auto task = scheduler.Add([] { return std::make_tuple(20, 21, 1); });
    auto next = task.Apply(SumThree);

    EXPECT_EQ(next.GetResultSync<int>(), 42);
}

TEST(Task, ApplyCreatesDependentTaskFromMultiArgumentLambda) {
    scheduler::TTaskScheduler scheduler;
    auto task =
        scheduler.Add([] { return std::make_tuple(std::string{"ab"}, 21); });
    auto next = task.Apply([](const std::string& prefix, int count) {
        return prefix.size() * count;
    });

    EXPECT_EQ(next.GetResultSync<std::size_t>(), 42U);
}

TEST(Task, ApplyCreatesDependentTaskFromConstMemberFunction) {
    scheduler::TTaskScheduler scheduler;
    Multiplier multiplier{6};
    auto task = scheduler.Add([] { return 7; });
    auto next = task.Apply(&Multiplier::Multiply, multiplier);

    EXPECT_EQ(next.GetResultSync<int>(), 42);
}

TEST(Task, ApplyCreatesDependentTaskFromMutableMemberFunction) {
    scheduler::TTaskScheduler scheduler;
    Multiplier multiplier{40};
    auto task = scheduler.Add([] { return 2; });
    auto next = task.Apply(&Multiplier::AddToFactor, multiplier);

    EXPECT_EQ(next.GetResultSync<int>(), 42);
}

TEST(Node, ExecuteDetectsRecursiveCycle) {
    scheduler::TFuture<int>* future = nullptr;
    auto recursive = [&future] { return future->Get(); };
    auto node =
        std::make_shared<scheduler::Node<decltype(recursive)>>(recursive);
    auto self_future = scheduler::TFuture<int>(node);
    future = &self_future;

    EXPECT_THROW(node->Execute(), scheduler::CycleDetected);
}

TEST(Node, MoveResultExpiresNodeResult) {
    auto node = std::make_shared<scheduler::Node<std::function<int()>>>(
        [] { return 42; });

    node->Execute();
    EXPECT_EQ(node->MoveResult(), 42);
    EXPECT_THROW(node->GetResultRef(), scheduler::AccessAfterMove);
    EXPECT_THROW(node->MoveResult(), scheduler::AccessAfterMove);
}
