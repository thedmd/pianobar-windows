# pragma once

template <typename T>
class com_ptr
{
//private:
//    typedef void (com_ptr::*bool_type)() const;
//    void safe_bool() const {}
//
//
public:
    com_ptr():
        _ptr(nullptr)
    {
    }

    com_ptr(T* ptr, bool add_ref = false):
        _ptr(ptr)
    {
        if (_ptr && add_ref)
            _ptr->AddRef();
    }

    com_ptr(const com_ptr& rhs):
        _ptr(rhs._ptr)
    {
        if (_ptr)
            _ptr->AddRef();
    }

    com_ptr(com_ptr&& rhs):
        _ptr(rhs._ptr)
    {
        rhs._ptr = nullptr;
    }

    template <typename U>
    com_ptr(const com_ptr<U>& rhs):
        _ptr(rhs._ptr)
    {
        if (_ptr)
            _ptr->AddRef();
    }

    ~com_ptr()
    {
        if (_ptr)
            _ptr->Release();
    }

    com_ptr& operator = (const com_ptr& rhs)
    {
        com_ptr(rhs).swap(*this);
        return *this;
    }

    com_ptr& operator = (com_ptr&& rhs)
    {
        com_ptr(static_cast<com_ptr&&>(rhs)).swap(*this);
        return *this;
    }

    template <typename U>
    com_ptr& operator = (const com_ptr<U>& rhs)
    {
        com_ptr(rhs).swap(*this):
        return *this;
    }

    com_ptr& operator = (T* rhs)
    {
        com_ptr(rhs).swap(*this);
        return *this;
    }

    //operator bool_type() const
    //{
    //    return _ptr ? &com_ptr::safe_bool : nullptr;
    //}

    //bool_type operator !() const
    //{
    //    return !((bool_type)*this);
    //}

    void reset()
    {
        com_ptr().swap(*this);
    }

    void reset(T* rhs)
    {
        com_ptr(rhs).swap(*this);
    }

    void reset(T* rhs, bool add_ref)
    {
        com_ptr(rhs, add_ref).swap(*this);
    }

    T* get() const
    {
        return _ptr;
    }

    T* detach()
    {
        auto result = _ptr;
        _ptr = nullptr;
        return result;
    }

    void swap(com_ptr& rhs)
    {
        T* temp = rhs._ptr;
        rhs._ptr = _ptr;
        _ptr = temp;
    }

    T&  operator *  () const { return *_ptr; }
    T*  operator -> () const { return  _ptr; }

        operator T* () const { return  _ptr; }
    T** operator &  ()       { return &_ptr; }

private:
    T*      _ptr;
};

template <typename T, typename U> inline bool operator==(const com_ptr<T>& a, const com_ptr<U>& b) { return a.get() == b.get(); }
template <typename T, typename U> inline bool operator!=(const com_ptr<T>& a, const com_ptr<U>& b) { return a.get() != b.get(); }
template <typename T, typename U> inline bool operator==(const com_ptr<T>& a, U* b) { return a.get() == b; }
template <typename T, typename U> inline bool operator!=(const com_ptr<T>& a, U* b) { return a.get() != b; }
template <typename T, typename U> inline bool operator==(T* a, const com_ptr<U>& b) { return a == b.get(); }
template <typename T, typename U> inline bool operator!=(T* a, const com_ptr<U>& b) { return a != b.get(); }
template <typename T> inline bool operator==(const com_ptr<T>& p, std::nullptr_t) { return p.get() == nullptr; }
template <typename T> inline bool operator==(std::nullptr_t, const com_ptr<T>& p) { return p.get() == nullptr; }
template <typename T> inline bool operator!=(const com_ptr<T>& p, std::nullptr_t) { return p.get() != nullptr; }
template <typename T> inline bool operator!=(std::nullptr_t, const com_ptr<T>& p) { return p.get() != nullptr; }
template <typename T> inline bool operator<(const com_ptr<T>& a, const com_ptr<T>& b) { return std::less<T*>()(a.get(), b.get()); }
template <typename T> inline bool operator<=(const com_ptr<T>& a, const com_ptr<T>& b) { return std::less_equal<T*>()(a.get(), b.get()); }
template <typename T> inline bool operator>(const com_ptr<T>& a, const com_ptr<T>& b) { return std::greater<T*>()(a.get(), b.get()); }
template <typename T> inline bool operator>=(const com_ptr<T>& a, const com_ptr<T>& b) { return std::greater_equal<T*>()(a.get(), b.get()); }
template <typename T> void swap(com_ptr<T> & lhs, com_ptr<T> & rhs) { lhs.swap(rhs); }
