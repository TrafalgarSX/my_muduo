#ifndef MUDUO_NET_ENTRYCOMPARE_H
#define MUDUO_NET_ENTRYCOMPARE_H

#include <type_traits>
#include <memory>
#include <base/Timestamp.h>

namespace muduo {
    
class Timer;

struct EntryCompareNormal
{
    using is_transparent = void;  // 启用异构查找
    
    bool operator()(const std::pair<Timestamp, std::unique_ptr<Timer>>& lhs,
                    const std::pair<Timestamp, std::unique_ptr<Timer>>& rhs) const
    {
        if (lhs.first != rhs.first) {
            return lhs.first < rhs.first;
        }
        return lhs.second.get() < rhs.second.get();
    }
    
    bool operator()(const std::pair<Timestamp, std::unique_ptr<Timer>>& lhs,
                    const std::pair<Timestamp, Timer*>& rhs) const
    {
        if (lhs.first != rhs.first) {
            return lhs.first < rhs.first;
        }
        return lhs.second.get() < rhs.second;
    }
    
     bool operator()(const std::pair<Timestamp, Timer*>& lhs,
                    const std::pair<Timestamp, std::unique_ptr<Timer>>& rhs) const
    {
        if (lhs.first != rhs.first) {
            return lhs.first < rhs.first;
        }
        return lhs.second < rhs.second.get();
    }
};

struct EntryCompareCXX14
{
    using is_transparent = void;

private:
    // SFINAE: 处理 unique_ptr 类型
    template<typename T>
    static auto getTimerPtr(const std::pair<Timestamp, T>& pair)
        -> typename std::enable_if<
            std::is_same<T, std::unique_ptr<Timer>>::value, 
            Timer*
        >::type
    {
        return pair.second.get();
    }
    
    // SFINAE: 处理原始指针类型
    template<typename T>
    static auto getTimerPtr(const std::pair<Timestamp, T>& pair)
        -> typename std::enable_if<
            std::is_same<T, Timer*>::value, 
            Timer*
        >::type
    {
        return pair.second;
    }

public:
    template<typename T, typename U>
    bool operator()(const T& lhs, const U& rhs) const {
        if (lhs.first != rhs.first) {
            return lhs.first < rhs.first;
        }
        return getTimerPtr(lhs) < getTimerPtr(rhs);
    }
};

struct EntryCompareCXX17
{
    using is_transparent = void;  // 启用异构查找

private:
    // 辅助函数：提取时间戳
    template<typename T>
    static const Timestamp& getTimestamp(const std::pair<Timestamp, T>& pair) {
        return pair.first;
    }
    
    // 辅助函数：提取Timer指针
    template<typename T>
    static Timer* getTimerPtr(const std::pair<Timestamp, T>& pair) {
        if constexpr (std::is_same_v<T, std::unique_ptr<Timer>>) {
            return pair.second.get();
        } else {
            return pair.second;
        }
    }

public:
    // 统一的模板比较函数
    template<typename T, typename U>
    bool operator()(const T& lhs, const U& rhs) const {
        // 确保两个参数都是 pair<Timestamp, XXX> 类型
        static_assert(std::is_same_v<typename T::first_type, Timestamp> && 
                     std::is_same_v<typename U::first_type, Timestamp>,
                     "Both arguments must be pairs with Timestamp as first type");
        
        // 先比较时间戳
        const Timestamp& lhs_time = getTimestamp(lhs);
        const Timestamp& rhs_time = getTimestamp(rhs);
        
        if (lhs_time != rhs_time) {
            return lhs_time < rhs_time;
        }
        
        // 时间戳相同时比较Timer指针地址
        return getTimerPtr(lhs) < getTimerPtr(rhs);
    }
};

}


#endif // MUDUO_NET_ENTRYCOMPARE_H