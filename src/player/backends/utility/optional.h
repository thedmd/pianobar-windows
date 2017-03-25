//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
# pragma once

# include <type_traits>
# include <utility>
# include <cassert>


//------------------------------------------------------------------------------
struct nullopt_t
{
    struct init {};
    nullopt_t(init) {}
};

const nullopt_t nullopt((nullopt_t::init()));

template <typename T>
struct optional
{
private:
    typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type storage_type;

    typedef void (optional::*bool_type)() const;
    void safe_bool() const { }


public:
    optional();
    optional(nullopt_t);
    optional(const optional& v);
    optional(optional&& v);
    optional(const T& v);
    optional(T&& v);
    ~optional();

    operator bool_type() const;

    optional& operator=(nullopt_t);

    optional& operator=(const optional& other);
    optional& operator=(optional&& other);

    // The function does not participate in overload resolution unless std::is_same<std::decay_t<U>, T>::value is true
    template <typename U>
    // optional& operator=(U&& value);
    optional& operator=(typename std::enable_if<std::is_same<typename std::decay<U>::type, T>::value, U>::type&& value);

          T* operator->();
    const T* operator->() const;
          T& operator*();
    const T& operator*() const;

          T& value();
    const T& value() const;

# if defined(_MSC_VER) && (_MSC_VER > 1600)
    template <typename U>
    T value_or(U&& default_value) const &;

    template <typename U>
    T value_or(U&& default_value) &&;
# else
    T value_or(const T& default_value);
    T value_or(const T& default_value) const;
# endif

    bool has_value() const;

    void swap(optional& other);

private:
          T* value_ptr()       { return reinterpret_cast<      T*>(&_value); }
    const T* value_ptr() const { return reinterpret_cast<const T*>(&_value); }

          T& value_ref()       { return *value_ptr(); }
    const T& value_ref() const { return *value_ptr(); }

    bool            _has_value;
    storage_type    _value;
# if defined(_DEBUG)
    const T&        _preview;
# endif
};


//------------------------------------------------------------------------------
template <typename T>
inline optional<T>::optional():
    _has_value(false)
# if defined(_DEBUG)
    ,_preview(value_ref())
# endif
{
}

template <typename T>
inline optional<T>::optional(nullopt_t):
    _has_value(false)
# if defined(_DEBUG)
    , _preview(value_ref())
# endif
{
}

template <typename T>
inline optional<T>::optional(const optional& v):
    _has_value(v._has_value)
# if defined(_DEBUG)
    , _preview(value_ref())
# endif
{
    if (_has_value)
        new (value_ptr()) T(v.value_ref());
}

template <typename T>
inline optional<T>::optional(optional&& v):
    _has_value(v._has_value)
# if defined(_DEBUG)
    , _preview(value_ref())
# endif
{
    if (!_has_value)
        return;

    new (value_ptr()) T(std::move(v.value_ref()));
    v.value_ref().~T();

    v._has_value = false;
}

template <typename T>
inline optional<T>::optional(const T& v):
    _has_value(true)
# if defined(_DEBUG)
    , _preview(value_ref())
# endif
{
    new (value_ptr()) T(v);
}

template <typename T>
inline optional<T>::optional(T&& v):
    _has_value(true)
# if defined(_DEBUG)
    , _preview(value_ref())
# endif
{
    new (value_ptr()) T(std::forward<T>(v));
}

template <typename T>
inline optional<T>::~optional()
{
    if (_has_value)
        value_ref().~T();
}

template <typename T>
inline optional<T>::operator bool_type() const
{
    return _has_value ? &optional::safe_bool : nullptr;
}

template <typename T>
inline optional<T>& optional<T>::operator= (nullopt_t)
{
    optional().swap(*this);
    _has_value = false;
    return *this;
}

template <typename T>
inline optional<T>& optional<T>::operator=(const optional& other)
{
    optional(other).swap(*this);
    return *this;
}

template <typename T>
inline optional<T>& optional<T>::operator=(optional&& other)
{
    optional(std::forward<optional>(other)).swap(*this);
    return *this;
}

template <typename T>
template <typename U>
inline optional<T>& optional<T>::operator=(typename std::enable_if<std::is_same<typename std::decay<U>::type, T>::value, U>::type&& value)
{
    optional<T>(value).swap(*this);
    return *this;
}


template <typename T>
inline T* optional<T>::operator->()
{
    assert(_has_value);
    return value_ptr();
}

template <typename T>
inline const T* optional<T>::operator->() const
{
    assert(_has_value);
    return value_ptr();
}

template <typename T>
inline T& optional<T>::operator*()
{
    assert(_has_value);
    return value_ref();
}

template <typename T>
inline const T& optional<T>::operator*() const
{
    assert(_has_value);
    return value_ref();
}

template <typename T>
inline T& optional<T>::value()
{
    assert(_has_value);
    return value_ref();
}

template <typename T>
inline const T& optional<T>::value() const
{
    assert(_has_value);
    return value_ref();
}

# if defined(_MSC_VER) && (_MSC_VER > 1600)
template <typename T>
template <typename U>
inline T optional<T>::value_or(U&& default_value) const &
{
    return bool(*this) ? value_ref() : static_cast<T>(std::forward<U>(default_value));
}

template <typename T>
template <typename U>
inline T optional<T>::value_or(U&& default_value) &&
{
    return bool(*this) ? std::move(value_ref()) : static_cast<T>(std::forward<U>(default_value));
}
# else
template <typename T>
T optional<T>::value_or(const T& default_value)
{
    return _has_value ? value_ref() : default_value;
}

template <typename T>
T optional<T>::value_or(const T& default_value) const
{
    return _has_value ? value_ref() : default_value;
}
# endif

template <typename T>
inline bool optional<T>::has_value() const
{
    return _has_value;
}

template <typename T>
inline void optional<T>::swap(optional& other)
{
    using std::swap;

    if (_has_value && other._has_value)
    {
        swap(value_ref(), other.value_ref());
    }
    else if (_has_value && !other._has_value)
    {
        new (other.value_ptr()) T(std::forward<T>(value_ref()));

        value_ref().~T();

        _has_value = false;
        other._has_value = true;
    }
    else if (!_has_value && other._has_value)
    {
        new (value_ptr()) T(std::forward<T>(other.value_ref()));

        other.value_ref().~T();

        _has_value = true;
        other._has_value = false;
    }
}


//------------------------------------------------------------------------------
template <typename T>
optional<T> make_optional(T&& v)
{
    return optional<T>(std::forward<T>(v));
}

template <typename T>
inline void swap(optional<T>& lhs, optional<T>& rhs)
{
    lhs.swap(rhs);
}

template <typename T>
inline bool operator==(const optional<T>& lhs, const optional<T>& rhs)
{
    if (static_cast<bool>(lhs) != static_cast<bool>(rhs))
        return false;
    if (!static_cast<bool>(lhs))
        return true;
    return *lhs == *rhs;
}

template <typename T>
bool operator!=(const optional<T>& lhs, const optional<T>& rhs)
{
    return !(lhs == rhs);
}

template <typename T>
inline bool operator<(const optional<T>& lhs, const optional<T>& rhs)
{
    if (!static_cast<bool>(rhs))
        return false;
    if (!static_cast<bool>(lhs))
        return true;
    return *lhs < *rhs;
}

template <typename T>
inline bool operator>(const optional<T>& lhs, const optional<T>& rhs)
{
    return rhs < lhs;
}

template <typename T>
inline bool operator<=(const optional<T>& lhs, const optional<T>& rhs)
{
    return !(rhs < lhs);
}

template <typename T>
inline bool operator>=(const optional<T>& lhs, const optional<T>& rhs)
{
    return !(lhs < rhs);
}

template <typename T>
inline bool operator==(const optional<T>& opt, nullopt_t)
{
    return !static_cast<bool>(opt);
}

template <typename T>
inline bool operator==(nullopt_t, const optional<T>& opt)
{
    return static_cast<bool>(opt);
}

template <typename T>
inline bool operator!=(const optional<T>& opt, nullopt_t)
{
    return static_cast<bool>(opt);
}

template <typename T>
inline bool operator!=(nullopt_t, const optional<T>& opt)
{
    return !static_cast<bool>(opt);
}

template <typename T>
inline bool operator<(const optional<T>& opt, nullopt_t)
{
    return false;
}

template <typename T>
inline bool operator<(nullopt_t, const optional<T>& opt)
{
    return static_cast<bool>(opt);
}

template <typename T>
inline bool operator<=(const optional<T>& opt, nullopt_t)
{
    return !opt;
}

template <typename T>
inline bool operator<=(nullopt_t, const optional<T>& opt)
{
    return true;
}

template <typename T>
inline bool operator>(const optional<T>& opt, nullopt_t)
{
    return static_cast<bool>(opt);
}

template <typename T>
inline bool operator>(nullopt_t, const optional<T>& opt)
{
    return false;
}

template <typename T>
inline bool operator>=(const optional<T>&, nullopt_t)
{
    return true;
}

template <typename T>
inline bool operator>=(nullopt_t, const optional<T>& opt)
{
    return !opt;
}

template <typename T>
inline bool operator==(const optional<T>& opt, const T& v)
{
    return static_cast<bool>(opt) ? *opt == v : false;
}

template <typename T>
inline bool operator==(const T& v, const optional<T>& opt)
{
    return static_cast<bool>(opt) ? *opt == v : false;
}

template <typename T>
inline bool operator!=(const optional<T>& opt, const T& v)
{
    return static_cast<bool>(opt) ? *opt != v : true;
}

template <typename T>
inline bool operator!=(const T& v, const optional<T>& opt)
{
    return static_cast<bool>(opt) ? *opt != v : true;
}

template <typename T>
inline bool operator<(const optional<T>& opt, const T& v)
{
    using namespace std;
    return static_cast<bool>(opt) ? less<T>(*opt, v) : true;
}

template <typename T>
inline bool operator<(const T& v, const optional<T>& opt)
{
    using namespace std;
    return static_cast<bool>(opt) ? less<T>(v, *opt) : false;
}

template <typename T>
inline bool operator<=(const optional<T>& opt, const T& v)
{
    using namespace std;
    return static_cast<bool>(opt) ? less_equal<T>(*opt, v) : true;
}

template <typename T>
inline bool operator<=(const T& v, const optional<T>& opt)
{
    using namespace std;
    return static_cast<bool>(opt) ? less_equal<T>(v, *opt) : false;
}

template <typename T>
inline bool operator>(const optional<T>& opt, const T& v)
{
    using namespace std;
    return static_cast<bool>(opt) ? greater<T>(*opt, v) : false;
}

template <typename T>
inline bool operator>(const T& v, const optional<T>& opt)
{
    using namespace std;
    return static_cast<bool>(opt) ? greater<T>(v, *opt) : true;
}

template <typename T>
inline bool operator>=(const optional<T>& opt, const T& v)
{
    using namespace std;
    return static_cast<bool>(opt) ? greater_equal<T>(*opt, v) : false;
}

template <typename T>
inline bool operator>=(const T& v, const optional<T>& opt)
{
    using namespace std;
    return static_cast<bool>(opt) ? greater_equal<T>(v, *opt) : true;
}
