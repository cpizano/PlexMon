// This is the plex precompiled header, not the same as the VC precompiled header.

#pragma once
#define NOMINMAX

#include <SDKDDKVer.h>





#include <string.h>
#include <array>
#include <iterator>
#include <algorithm>
#include <limits>
#include <type_traits>
#include <string>
#include <memory>
#include <vector>
#include <stdarg.h>
#include <windows.h>








///////////////////////////////////////////////////////////////////////////////
// plx::Exception
// line_ : The line of code, usually __LINE__.
// message_ : Whatever useful text.
//
namespace plx {
class Exception {
  int line_;
  const char* message_;

protected:
  void PostCtor() {
    if (::IsDebuggerPresent()) {
      //__debugbreak();
    }
  }

public:
  Exception(int line, const char* message) : line_(line), message_(message) {}
  virtual ~Exception() {}
  const char* Message() const { return message_; }
  int Line() const { return line_; }
};


///////////////////////////////////////////////////////////////////////////////
// plx::RangeException (thrown by ItRange and others)
//
class RangeException : public plx::Exception {
  void* ptr_;
public:
  RangeException(int line, void* ptr)
      : Exception(line, "Invalid Range"), ptr_(ptr) {
    PostCtor();
  }

  void* pointer() const {
    return ptr_;
  }
};



///////////////////////////////////////////////////////////////////////////////
// HexASCII (converts a byte into a two-char readable representation.
//
static const char HexASCIITable[] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

char* HexASCII(uint8_t byte, char* out) ;




///////////////////////////////////////////////////////////////////////////////
// plx::ItRange
// s_ : first element
// e_ : one past the last element
//
template <typename It>
class ItRange {
  It s_;
  It e_;

public:
  typedef typename std::iterator_traits<
      typename std::remove_reference<It>::type
  >::reference RefT;

  typedef typename std::iterator_traits<
      typename std::remove_reference<It>::type
  >::value_type ValueT;

  typedef typename std::remove_const<It>::type NoConstIt;

  ItRange() : s_(), e_() {
  }

  template <typename U>
  ItRange(const ItRange<U>& other) : s_(other.start()), e_(other.end()) {
  }

  ItRange(It start, It end) : s_(start), e_(end) {
  }

  ItRange(It start, size_t size) : s_(start), e_(start + size) {
  }

  bool empty() const {
    return (s_ == e_);
  }

  size_t size() const {
    return (e_ - s_);
  }

  It start() const {
    return s_;
  }

  It begin() const {
    return s_;
  }

  It end() const {
    return e_;
  }

  bool valid() const {
    return (e_ >= s_);
  }

  RefT front() const {
    if (s_ >= e_)
      throw plx::RangeException(__LINE__, nullptr);
    return s_[0];
  }

  RefT back() const {
    return e_[-1];
  }

  RefT operator[](size_t i) const {
    return s_[i];
  }

  bool equals(const ItRange<It>& o) const {
    if (o.size() != size())
      return false;
    return (memcmp(s_, o.s_, size()) == 0);
  }

  size_t starts_with(const ItRange<It>& o) const {
    if (o.size() > size())
      return 0;
    return (memcmp(s_, o.s_, o.size()) == 0) ? o.size() : 0;
  }

  bool contains(const uint8_t* ptr) const {
    return ((ptr >= reinterpret_cast<uint8_t*>(s_)) &&
            (ptr < reinterpret_cast<uint8_t*>(e_)));
  }

  bool contains(ValueT x, size_t* pos) const {
    auto c = s_;
    while (c != e_) {
      if (*c == x) {
        *pos = c - s_;
        return true;
      }
      ++c;
    }
    return false;
  }

  template <size_t count>
  size_t CopyToArray(ValueT (&arr)[count]) const {
    auto copied = std::min(size(), count);
    auto last = copied + s_;
    std::copy(s_, last, arr);
    return copied;
  }

  template <size_t count>
  size_t CopyToArray(std::array<ValueT, count>& arr) const {
    auto copied = std::min(size(), count);
    auto last = copied + s_;
    std::copy(s_, last, arr.begin());
    return copied;
  }

  intptr_t advance(size_t count) {
    auto ns = s_ + count;
    if (ns > e_)
      return (e_ - ns);
    s_ = ns;
    return size();
  }

  void clear() {
    s_ = It();
    e_ = It();
  }

  void reset_start(It new_start) {
    auto sz = size();
    s_ = new_start;
    e_ = s_ + sz;
  }

  void extend(size_t count) {
    e_ += count;
  }

  ItRange<const uint8_t*> const_bytes() const {
    auto s = reinterpret_cast<const uint8_t*>(s_);
    auto e = reinterpret_cast<const uint8_t*>(e_);
    return ItRange<const uint8_t*>(s, e);
  }

  ItRange<uint8_t*> bytes() const {
    auto s = reinterpret_cast<uint8_t*>(s_);
    auto e = reinterpret_cast<uint8_t*>(e_);
    return ItRange<uint8_t*>(s, e);
  }

  ItRange<It> slice(size_t start, size_t count = 0) const {
    return ItRange<It>(s_ + start,
                       count ? (s_ + start + count) : e_ );
  }

};

template <typename U, size_t count>
ItRange<U*> RangeFromLitStr(U (&str)[count]) {
  return ItRange<U*>(str, str + count - 1);
}

template <typename U, size_t count>
ItRange<U*> RangeFromArray(U (&str)[count]) {
  return ItRange<U*>(str, str + count);
}

template <typename U>
ItRange<U*> RangeUntilValue(U* start, U value) {
  auto stop = start;
  while (*stop != value) {
    ++stop;
  }
  return ItRange<U*>(start, stop);
}

template <typename U>
ItRange<U*> RangeFromVector(std::vector<U>& vec, size_t len = 0) {
  auto s = &vec[0];
  return ItRange<U*>(s, len ? s + len : s + vec.size());
}

template <typename U>
ItRange<const U*> RangeFromVector(const std::vector<U>& vec, size_t len = 0) {
  auto s = &vec[0];
  return ItRange<const U*>(s, len ? s + len : s + vec.size());
}

ItRange<uint8_t*> RangeFromBytes(void* start, size_t count) ;

ItRange<const uint8_t*> RangeFromBytes(const void* start, size_t count) ;

ItRange<const uint8_t*> RangeFromString(const std::string& str) ;

ItRange<uint8_t*> RangeFromString(std::string& str) ;

ItRange<const uint16_t*> RangeFromString(const std::wstring& str) ;

ItRange<uint16_t*> RangeFromString(std::wstring& str) ;

template <typename U>
std::string StringFromRange(const ItRange<U>& r) {
  return std::string(r.start(), r.end());
}

template <typename U>
std::wstring WideStringFromRange(const ItRange<U>& r) {
  return std::wstring(r.start(), r.end());
}

template <typename U>
std::unique_ptr<U[]> HeapRange(ItRange<U*>&r) {
  std::unique_ptr<U[]> ptr(new U[r.size()]);
  r.reset_start(ptr.get());
  return ptr;
}


///////////////////////////////////////////////////////////////////////////////
// plx::Range  (alias for ItRange<T*>)
//
template <typename T>
using Range = plx::ItRange<T*>;


///////////////////////////////////////////////////////////////////////////////
// plx::OverflowKind
//
enum class OverflowKind {
  None,
  Positive,
  Negative,
};


///////////////////////////////////////////////////////////////////////////////
// plx::OverflowException (thrown by some numeric converters)
// kind_ : Type of overflow, positive or negative.
//
class OverflowException : public plx::Exception {
  plx::OverflowKind kind_;

public:
  OverflowException(int line, plx::OverflowKind kind)
      : Exception(line, "Overflow"), kind_(kind) {
    PostCtor();
  }
  plx::OverflowKind kind() const { return kind_; }
};


///////////////////////////////////////////////////////////////////////////////
// plx::NextInt  integer promotion.

short NextInt(char value) ;

int NextInt(short value) ;

long long NextInt(int value) ;

long long NextInt(long value) ;

long long NextInt(long long value) ;

short NextInt(unsigned char value) ;

int NextInt(unsigned short value) ;

long long NextInt(unsigned int value) ;

long long NextInt(unsigned long value) ;

long long NextInt(unsigned long long value) ;


///////////////////////////////////////////////////////////////////////////////
std::string HexASCIIStr(const plx::Range<const uint8_t>& r, char separator) ;


///////////////////////////////////////////////////////////////////////////////
// plx::CodecException (thrown by some decoders)
// bytes_ : The 16 bytes or less that caused the issue.
//
class CodecException : public plx::Exception {
  uint8_t bytes_[16];
  size_t count_;

public:
  CodecException(int line, const plx::Range<const unsigned char>* br)
      : Exception(line, "Codec exception"), count_(0) {
    if (br)
      count_ = br->CopyToArray(bytes_);
    PostCtor();
  }

  std::string bytes() const {
    return plx::HexASCIIStr(plx::Range<const uint8_t>(bytes_, count_), ',');
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::StringPrintf  (c-style printf for std strings)
//

std::string StringPrintf(const char* fmt, ...) ;


///////////////////////////////////////////////////////////////////////////////
// plx::To  (integer to integer type safe cast)
//

template <bool src_signed, bool tgt_signed>
struct ToCastHelper;

template <>
struct ToCastHelper<false, false> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (sizeof(Tgt) >= sizeof(Src)) {
      return static_cast<Tgt>(value);
    } else {
      if (value > std::numeric_limits<Tgt>::max())
        throw plx::OverflowException(__LINE__, OverflowKind::Positive);
      if (value < std::numeric_limits<Tgt>::min())
        throw plx::OverflowException(__LINE__, OverflowKind::Negative);
      return static_cast<Tgt>(value);
    }
  }
};

template <>
struct ToCastHelper<true, true> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (sizeof(Tgt) >= sizeof(Src)) {
      return static_cast<Tgt>(value);
    } else {
      if (value > std::numeric_limits<Tgt>::max())
        throw plx::OverflowException(__LINE__, OverflowKind::Positive);
      if (value < std::numeric_limits<Tgt>::min())
        throw plx::OverflowException(__LINE__, OverflowKind::Negative);
      return static_cast<Tgt>(value);
    }
  }
};

template <>
struct ToCastHelper<false, true> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (plx::NextInt(value) > std::numeric_limits<Tgt>::max())
      throw plx::OverflowException(__LINE__, OverflowKind::Positive);
    if (plx::NextInt(value) < std::numeric_limits<Tgt>::min())
      throw plx::OverflowException(__LINE__, OverflowKind::Negative);
    return static_cast<Tgt>(value);
  }
};

template <>
struct ToCastHelper<true, false> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (value < Src(0))
      throw plx::OverflowException(__LINE__, OverflowKind::Negative);
    if (unsigned(value) > std::numeric_limits<Tgt>::max())
      throw plx::OverflowException(__LINE__, OverflowKind::Positive);
    return static_cast<Tgt>(value);
  }
};

template <typename Tgt, typename Src>
typename std::enable_if<
    std::numeric_limits<Tgt>::is_integer &&
    std::numeric_limits<Src>::is_integer,
    Tgt>::type
To(const Src & value) {
  return ToCastHelper<std::numeric_limits<Src>::is_signed,
                      std::numeric_limits<Tgt>::is_signed>::cast<Tgt>(value);
}


///////////////////////////////////////////////////////////////////////////////
// plx::UTF16FromUTF8
std::wstring UTF16FromUTF8(const plx::Range<const uint8_t>& utf8, bool strict) ;
}

extern "C" IMAGE_DOS_HEADER __ImageBase;
