/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_REF_BASE_H
#define ANDROID_REF_BASE_H

#include <cutils/atomic.h>
#include <utils/TextOutput.h>

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
namespace android {

template<typename T> class wp;

// ---------------------------------------------------------------------------

#define COMPARE(_op_)                                           \
inline bool operator _op_ (const sp<T>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
inline bool operator _op_ (const wp<T>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
inline bool operator _op_ (const T* o) const {                  \
    return m_ptr _op_ o;                                        \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const sp<U>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const wp<U>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const U* o) const {                  \
    return m_ptr _op_ o;                                        \
}

// ---------------------------------------------------------------------------

class RefBase
{
public:     /*  incStrong 和 decStrong 维护它所引用的 
                weakref_impl 对象的引用计数。 
            */
            void            incStrong(const void* id) const;
            void            decStrong(const void* id) const;
    
            void            forceIncStrong(const void* id) const;

            //! DEBUGGING ONLY: Get current strong ref count.
            int32_t         getStrongCount() const;
    /* weakref_type 只定义了引用计数维护接口，具体的实现是由 weakref_impl 类提供的 */
    class weakref_type
    {
    public:
        RefBase*            refBase() const;
        
        void                incWeak(const void* id);
        void                decWeak(const void* id);
        
        bool                attemptIncStrong(const void* id);
        
        //! This is only safe if you have set OBJECT_LIFETIME_FOREVER.
        bool                attemptIncWeak(const void* id);

        //! DEBUGGING ONLY: Get current weak ref count.
        int32_t             getWeakCount() const;

        //! DEBUGGING ONLY: Print references held on object.
        void                printRefs() const;

        //! DEBUGGING ONLY: Enable tracking for this object.
        // enable -- enable/disable tracking
        // retain -- when tracking is enable, if true, then we save a stack trace
        //           for each reference and dereference; when retain == false, we
        //           match up references and dereferences and keep only the 
        //           outstanding ones.
        
        void                trackMe(bool enable, bool retain);
    };
    
            weakref_type*   createWeak(const void* id) const;
            
            weakref_type*   getWeakRefs() const;

            //! DEBUGGING ONLY: Print references held on object.
    inline  void            printRefs() const { getWeakRefs()->printRefs(); }

            //! DEBUGGING ONLY: Enable tracking of object.
    inline  void            trackMe(bool enable, bool retain)
    { 
        getWeakRefs()->trackMe(enable, retain); 
    }

protected:
                            RefBase();
    virtual                 ~RefBase();
    
    //! Flags for extendObjectLifetime()
    enum {
        OBJECT_LIFETIME_WEAK    = 0x0001,
        OBJECT_LIFETIME_FOREVER = 0x0003
    };
    
            void            extendObjectLifetime(int32_t mode);
            
    //! Flags for onIncStrongAttempted()
    enum {
        FIRST_INC_STRONG = 0x0001
    };
    
    virtual void            onFirstRef();
    virtual void            onLastStrongRef(const void* id);
    virtual bool            onIncStrongAttempted(uint32_t flags, const void* id);
    virtual void            onLastWeakRef(const void* id);

private:
    friend class weakref_type;
    class weakref_impl;
    
                            RefBase(const RefBase& o);
            RefBase&        operator=(const RefBase& o);
            
        weakref_impl* const mRefs;
};

// ---------------------------------------------------------------------------
/*  轻量级指针通过简单的引用计数技术来维护对象的生命周期. 
    模板参数 T 表示对象的实际类型，它必须是继承自 LightRefBase 类的类.
*/
template <class T>
class LightRefBase
{
public:
    inline LightRefBase() : mCount(0) { }
    /* 增加对象的引用计数. */
    inline void incStrong(const void* id) const {
        android_atomic_inc(&mCount);
    }
    /* 减少对象的引用计数. */
    inline void decStrong(const void* id) const {
        if (android_atomic_dec(&mCount) == 1) {
            delete static_cast<const T*>(this);
        }
    }
    //! DEBUGGING ONLY: Get current strong ref count.
    inline int32_t getStrongCount() const {
        return mCount;
    }
    
protected:
    inline ~LightRefBase() { }
    
private:
    /* 对象的引用计数值. */
    mutable volatile int32_t mCount;
};

// ---------------------------------------------------------------------------
/*  强指针类，同时也是轻量级指针类的模板实例化版本.
    模板参数 T 表示对象的实际类型，它必须是继承自 LightRefBase 类的类.
*/
template <typename T>
class sp
{
public:
    typedef typename RefBase::weakref_type weakref_type;
    
    inline sp() : m_ptr(0) { }

    sp(T* other);
    sp(const sp<T>& other);
    template<typename U> sp(U* other);
    template<typename U> sp(const sp<U>& other);

    ~sp();
    
    // Assignment

    sp& operator = (T* other);
    sp& operator = (const sp<T>& other);
    
    template<typename U> sp& operator = (const sp<U>& other);
    template<typename U> sp& operator = (U* other);
    
    //! Special optimization for use by ProcessState (and nobody else).
    void force_set(T* other);
    
    // Reset
    
    void clear();
    
    // Accessors

    inline  T&      operator* () const  { return *m_ptr; }
    inline  T*      operator-> () const { return m_ptr;  }
    inline  T*      get() const         { return m_ptr; }

    // Operators
        
    COMPARE(==)
    COMPARE(!=)
    COMPARE(>)
    COMPARE(<)
    COMPARE(<=)
    COMPARE(>=)

private:    
    template<typename Y> friend class sp;
    template<typename Y> friend class wp;

    // Optimization for wp::promote().
    /*  在内部使用 RefBase::weakref_type 类来维护它所引用的对象
        的强引用计数和弱引用计数.
    */
    sp(T* p, weakref_type* refs);
    /* m_ptr 指向实际引用的对象. */
    T*              m_ptr;
};

template <typename T>
TextOutput& operator<<(TextOutput& to, const sp<T>& val);

// ---------------------------------------------------------------------------
/* 模板参数 T 表示对象的实际类型，它必须是从 RefBase 类继承下来的. */
template <typename T>
class wp
{
public:
    typedef typename RefBase::weakref_type weakref_type;
    
    inline wp() : m_ptr(0) { }

    wp(T* other);
    wp(const wp<T>& other);
    wp(const sp<T>& other);
    template<typename U> wp(U* other);
    template<typename U> wp(const sp<U>& other);
    template<typename U> wp(const wp<U>& other);

    ~wp();
    
    // Assignment

    wp& operator = (T* other);
    wp& operator = (const wp<T>& other);
    wp& operator = (const sp<T>& other);
    
    template<typename U> wp& operator = (U* other);
    template<typename U> wp& operator = (const wp<U>& other);
    template<typename U> wp& operator = (const sp<U>& other);
    
    void set_object_and_refs(T* other, weakref_type* refs);

    // promotion to sp
    
    sp<T> promote() const;

    // Reset
    
    void clear();

    // Accessors
    
    inline  weakref_type* get_refs() const { return m_refs; }
    
    inline  T* unsafe_get() const { return m_ptr; }

    // Operators
        
    COMPARE(==)
    COMPARE(!=)
    COMPARE(>)
    COMPARE(<)
    COMPARE(<=)
    COMPARE(>=)

private:
    template<typename Y> friend class sp;
    template<typename Y> friend class wp;

    T*              m_ptr;  /* 指向它所引用的对象. */
    weakref_type*   m_refs; /* 维护对象的弱引用计数. */
};

template <typename T>
TextOutput& operator<<(TextOutput& to, const wp<T>& val);

#undef COMPARE

// ---------------------------------------------------------------------------
// No user serviceable parts below here.

template<typename T>
sp<T>::sp(T* other)
    : m_ptr(other)
{
    /*  因为 T 继承了 LightRefBase/RefBase, m_ptr 指向了对象 T, 
        这里实际调用了 LightRefBase/RefBase::incStrong() 来增加对象的引用计数.
    */
    if (other) other->incStrong(this);
}

template<typename T>
sp<T>::sp(const sp<T>& other)
    : m_ptr(other.m_ptr)
{   /* 这里与上面同理. */
    if (m_ptr) m_ptr->incStrong(this);
}

template<typename T> template<typename U>
sp<T>::sp(U* other) : m_ptr(other)
{
    if (other) other->incStrong(this);
}

template<typename T> template<typename U>
sp<T>::sp(const sp<U>& other)
    : m_ptr(other.m_ptr)
{
    if (m_ptr) m_ptr->incStrong(this);
}

template<typename T>
sp<T>::~sp()
{   /*  因为 T 继承了 LightRefBase/RefBase, m_ptr 指向了对象 T, 这里实际调用了 
        LightRefBase/RefBase::decStrong() 来减少对象的引用计数. 
    */
    if (m_ptr) m_ptr->decStrong(this);
}

template<typename T>
sp<T>& sp<T>::operator = (const sp<T>& other) {
    T* otherPtr(other.m_ptr);
    if (otherPtr) otherPtr->incStrong(this);
    if (m_ptr) m_ptr->decStrong(this);
    m_ptr = otherPtr;
    return *this;
}

template<typename T>
sp<T>& sp<T>::operator = (T* other)
{
    if (other) other->incStrong(this);
    if (m_ptr) m_ptr->decStrong(this);
    m_ptr = other;
    return *this;
}

template<typename T> template<typename U>
sp<T>& sp<T>::operator = (const sp<U>& other)
{
    U* otherPtr(other.m_ptr);
    if (otherPtr) otherPtr->incStrong(this);
    if (m_ptr) m_ptr->decStrong(this);
    m_ptr = otherPtr;
    return *this;
}

template<typename T> template<typename U>
sp<T>& sp<T>::operator = (U* other)
{
    if (other) other->incStrong(this);
    if (m_ptr) m_ptr->decStrong(this);
    m_ptr = other;
    return *this;
}

template<typename T>    
void sp<T>::force_set(T* other)
{
    other->forceIncStrong(this);
    m_ptr = other;
}

template<typename T>
void sp<T>::clear()
{
    if (m_ptr) {
        m_ptr->decStrong(this);
        m_ptr = 0;
    }
}
 /* p 指向对象地址, refs 指向该对象内部的弱引用计数.对象地址不为NULL的情况下，
    调用它内部的弱引用计数器对象的成员函数 attemptIncStrong 来试图增加该对象
    的强引用计数,实际调用 weakref_type::attemptIncStrong().
 */
template<typename T>
sp<T>::sp(T* p, weakref_type* refs)
    : m_ptr((p && refs->attemptIncStrong(this)) ? p : 0)
{
}

template <typename T>
inline TextOutput& operator<<(TextOutput& to, const sp<T>& val)
{
    to << "sp<>(" << val.get() << ")";
    return to;
}

// ---------------------------------------------------------------------------

template<typename T>
wp<T>::wp(T* other)
    : m_ptr(other)
{   /* T 是继承了 RefBase 类的子类, 这里实际调用 RefBase::createWeak() */
    if (other) m_refs = other->createWeak(this);
}

template<typename T>
wp<T>::wp(const wp<T>& other)
    : m_ptr(other.m_ptr), m_refs(other.m_refs)
{
    if (m_ptr) m_refs->incWeak(this);
}

template<typename T>
wp<T>::wp(const sp<T>& other)
    : m_ptr(other.m_ptr)
{
    if (m_ptr) {
        m_refs = m_ptr->createWeak(this);
    }
}

template<typename T> template<typename U>
wp<T>::wp(U* other)
    : m_ptr(other)
{
    if (other) m_refs = other->createWeak(this);
}

template<typename T> template<typename U>
wp<T>::wp(const wp<U>& other)
    : m_ptr(other.m_ptr)
{
    if (m_ptr) {
        m_refs = other.m_refs;
        m_refs->incWeak(this);
    }
}

template<typename T> template<typename U>
wp<T>::wp(const sp<U>& other)
    : m_ptr(other.m_ptr)
{
    if (m_ptr) {
        m_refs = m_ptr->createWeak(this);
    }
}

template<typename T>
wp<T>::~wp()
{
    /*  调用 m_refs 的成员函数 decWeak 来减少对象的弱引用计数.
        实际调用 weakref_impl::decWeak()
    */
    if (m_ptr) m_refs->decWeak(this);
}

template<typename T>
wp<T>& wp<T>::operator = (T* other)
{
    weakref_type* newRefs =
        other ? other->createWeak(this) : 0;
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = other;
    m_refs = newRefs;
    return *this;
}

template<typename T>
wp<T>& wp<T>::operator = (const wp<T>& other)
{
    weakref_type* otherRefs(other.m_refs);
    T* otherPtr(other.m_ptr);
    if (otherPtr) otherRefs->incWeak(this);
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = otherPtr;
    m_refs = otherRefs;
    return *this;
}

template<typename T>
wp<T>& wp<T>::operator = (const sp<T>& other)
{
    weakref_type* newRefs =
        other != NULL ? other->createWeak(this) : 0;
    T* otherPtr(other.m_ptr);
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = otherPtr;
    m_refs = newRefs;
    return *this;
}

template<typename T> template<typename U>
wp<T>& wp<T>::operator = (U* other)
{
    weakref_type* newRefs =
        other ? other->createWeak(this) : 0;
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = other;
    m_refs = newRefs;
    return *this;
}

template<typename T> template<typename U>
wp<T>& wp<T>::operator = (const wp<U>& other)
{
    weakref_type* otherRefs(other.m_refs);
    U* otherPtr(other.m_ptr);
    if (otherPtr) otherRefs->incWeak(this);
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = otherPtr;
    m_refs = otherRefs;
    return *this;
}

template<typename T> template<typename U>
wp<T>& wp<T>::operator = (const sp<U>& other)
{
    weakref_type* newRefs =
        other != NULL ? other->createWeak(this) : 0;
    U* otherPtr(other.m_ptr);
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = otherPtr;
    m_refs = newRefs;
    return *this;
}

template<typename T>
void wp<T>::set_object_and_refs(T* other, weakref_type* refs)
{
    if (other) refs->incWeak(this);
    if (m_ptr) m_refs->decWeak(this);
    m_ptr = other;
    m_refs = refs;
}

template<typename T>
sp<T> wp<T>::promote() const
{   /* 弱指针通过其内部的成员变量 m_ptr 和 m_refs 来创建一个强指针.*/
    return sp<T>(m_ptr, m_refs);
}

template<typename T>
void wp<T>::clear()
{
    if (m_ptr) {
        m_refs->decWeak(this);
        m_ptr = 0;
    }
}

template <typename T>
inline TextOutput& operator<<(TextOutput& to, const wp<T>& val)
{
    to << "wp<>(" << val.unsafe_get() << ")";
    return to;
}

}; // namespace android

// ---------------------------------------------------------------------------

#endif // ANDROID_REF_BASE_H
