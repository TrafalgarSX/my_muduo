#ifndef MUDUO_BASE_TIMESTAMP_DATE_H
#define MUDUO_BASE_TIMESTAMP_DATE_H

#include <date/date.h>
#include <chrono>
#include <string>

namespace muduo
{

///
/// Time stamp in UTC, in microseconds resolution using date::date library.
///
/// This class is immutable.
/// It's recommended to pass it by value, since it's passed in register on x64.
///
class Timestamp
{
 public:
  using time_point_type = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;
  
  ///
  /// Constructs an invalid Timestamp.
  ///
  Timestamp()
    : tp_(time_point_type::min())
  {
  }

  ///
  /// Constructs a Timestamp at specific time
  ///
  /// @param microSecondsSinceEpoch
  explicit Timestamp(int64_t microSecondsSinceEpochArg)
    : tp_(time_point_type(std::chrono::microseconds(microSecondsSinceEpochArg)))
  {
  }

  ///
  /// Constructs from chrono time_point
  ///
  explicit Timestamp(const time_point_type& tp)
    : tp_(tp)
  {
  }

  void swap(Timestamp& that)
  {
    std::swap(tp_, that.tp_);
  }

  // default copy/assignment/dtor are Okay

  ///
  /// Convert to string representation
  ///
  std::string toString() const
  {
    if (!valid()) {
      return "Invalid Timestamp";
    }
    
    auto days_part = date::floor<date::days>(tp_);
    auto time_part = tp_ - days_part;
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(time_part);
    
    return date::format("%F %T", days_part) + 
           "." + std::to_string(microseconds.count() % 1000000);
  }

  ///
  /// Convert to formatted string
  ///
  std::string toFormattedString(bool showMicroseconds = true) const
  {
    if (!valid()) {
      return "Invalid Timestamp";
    }
    
    if (showMicroseconds) {
      return toString();
    } else {
      return date::format("%F %T", date::floor<std::chrono::seconds>(tp_));
    }
  }

  bool valid() const { return tp_ != time_point_type::min(); }

  // for internal usage.
  int64_t microSecondsSinceEpoch() const 
  { 
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp_.time_since_epoch()).count(); 
  }
  
  time_t secondsSinceEpoch() const
  { 
    return std::chrono::duration_cast<std::chrono::seconds>(
        tp_.time_since_epoch()).count(); 
  }

  ///
  /// Get underlying time_point
  ///
  const time_point_type& timePoint() const { return tp_; }

  ///
  /// Get time of now.
  ///
  static Timestamp now()
  {
    auto now = std::chrono::system_clock::now();
    auto micros = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    return Timestamp(micros);
  }
  
  static Timestamp invalid()
  {
    return Timestamp();
  }

  static Timestamp fromUnixTime(time_t t)
  {
    return fromUnixTime(t, 0);
  }

  static Timestamp fromUnixTime(time_t t, int microseconds)
  {
    auto tp = std::chrono::system_clock::from_time_t(t);
    auto micros_tp = std::chrono::time_point_cast<std::chrono::microseconds>(tp);
    micros_tp += std::chrono::microseconds(microseconds);
    return Timestamp(micros_tp);
  }

  static const int kMicroSecondsPerSecond = 1000 * 1000;

 private:
  time_point_type tp_;
};

inline bool operator<(const Timestamp& lhs, const Timestamp& rhs)
{
  return lhs.timePoint() < rhs.timePoint();
}

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs)
{
  return lhs.timePoint() == rhs.timePoint();
}

inline bool operator!=(const Timestamp& lhs, const Timestamp& rhs)
{
  return !(lhs == rhs);
}

inline bool operator<=(const Timestamp& lhs, const Timestamp& rhs)
{
  return lhs < rhs || lhs == rhs;
}

inline bool operator>(const Timestamp& lhs, const Timestamp& rhs)
{
  return !(lhs <= rhs);
}

inline bool operator>=(const Timestamp& lhs, const Timestamp& rhs)
{
  return !(lhs < rhs);
}

///
/// Gets time difference of two timestamps, result in seconds.
///
/// @param high, low
/// @return (high-low) in seconds
/// @c double has 52-bit precision, enough for one-microsecond
/// resolution for next 100 years.
inline double timeDifference(const Timestamp& high, const Timestamp& low)
{
  auto diff = high.timePoint() - low.timePoint();
  return std::chrono::duration_cast<std::chrono::duration<double>>(diff).count();
}

///
/// Add @c seconds to given timestamp.
///
/// @return timestamp+seconds as Timestamp
///
inline Timestamp addTime(const Timestamp& timestamp, double seconds)
{
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::duration<double>(seconds));
  return Timestamp(timestamp.timePoint() + duration);
}

///
/// Subtract @c seconds from given timestamp.
///
/// @return timestamp-seconds as Timestamp
///
inline Timestamp subtractTime(const Timestamp& timestamp, double seconds)
{
  return addTime(timestamp, -seconds);
}

}  // namespace muduo

#endif  // MUDUO_BASE_TIMESTAMP_DATE_H