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

#define LOG_TAG "RefBase"

#include <utils/RefBase.h>

#include <utils/Atomic.h>
#include <utils/CallStack.h>
#include <utils/KeyedVector.h>
#include <utils/Log.h>
#include <utils/threads.h>

#include <stdlib.h>
#include <stdio.h>
#include <typeinfo>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// compile with refcounting debugging enabled
#define DEBUG_REFS                      0
#define DEBUG_REFS_ENABLED_BY_DEFAULT   1
#define DEBUG_REFS_CALLSTACK_ENABLED    1

// log all reference counting operations
#define PRINT_REFS                      0

// ---------------------------------------------------------------------------

namespace android {

#define INITIAL_STRONG_VALUE (1<<28)

// ---------------------------------------------------------------------------

class RefBase::weakref_impl : public RefBase::weakref_type
{
public:
    volatile int32_t    mStrong;    // 强引用计数.
    volatile int32_t    mWeak;      // 弱引用计数.
    RefBase* const      mBase;      // 引用的对象的地址.
    volatile int32_t    mFlags;     // 对象的生命周期控制方式标志.


#if !DEBUG_REFS

    weakref_impl(RefBase* base)
        : mStrong(INITIAL_STRONG_VALUE)
        , mWeak(0)
        , mBase(base)
        , mFlags(0)
    {
    }

    void addStrongRef(const void* /*id*/) { }
    void removeStrongRef(const void* /*id*/) { }
    void addWeakRef(const void* /*id*/) { }
    void removeWeakRef(const void* /*id*/) { }
    void printRefs() const { }
    void trackMe(bool, bool) { }

#else

    weakref_impl(RefBase* base)
        : mStrong(INITIAL_STRONG_VALUE)
        , mWeak(0)
        , mBase(base)
        , mFlags(0)
        , mStrongRefs(NULL)
        , mWeakRefs(NULL)
        , mTrackEnabled(!!DEBUG_REFS_ENABLED_BY_DEFAULT)
        , mRetain(false)
    {
        //LOGI("NEW weakref_impl %p for RefBase %p", this, base);
    }
    
    ~weakref_impl()
    {
        LOG_ALWAYS_FATAL_IF(!mRetain && mStrongRefs != NULL, "Strong references remain!");
        LOG_ALWAYS_FATAL_IF(!mRetain && mWeakRefs != NULL, "Weak references remain!");
    }

    void addStrongRef(const void* id)
    {
        addRef(&mStrongRefs, id, mStrong);
    }

    void removeStrongRef(const void* id)
    {
        if (!mRetain)
            removeRef(&mStrongRefs, id);
        else
            addRef(&mStrongRefs, id, -mStrong);
    }

    void addWeakRef(const void* id)
    {
        addRef(&mWeakRefs, id, mWeak);
    }

    void removeWeakRef(const void* id)
    {
        if (!mRetain)
            removeRef(&mWeakRefs, id);
        else
            addRef(&mWeakRefs, id, -mWeak);
    }

    void trackMe(bool track, bool retain)
    { 
        mTrackEnabled = track;
        mRetain = retain;
    }

    void printRefs() const
    {
        String8 text;

        {
            AutoMutex _l(const_cast<weakref_impl*>(this)->mMutex);
    
            char buf[128];
            sprintf(buf, "Strong references on RefBase %p (weakref_type %p):\n", mBase, this);
            text.append(buf);
            printRefsLocked(&text, mStrongRefs);
            sprintf(buf, "Weak references on RefBase %p (weakref_type %p):\n", mBase, this);
            text.append(buf);
            printRefsLocked(&text, mWeakRefs);
        }

        {
            char name[100];
            snprintf(name, 100, "/data/%p.stack", this);
            int rc = open(name, O_RDWR | O_CREAT | O_APPEND);
            if (rc >= 0) {
                write(rc, text.string(), text.length());
                close(rc);
                LOGD("STACK TRACE for %p saved in %s", this, name);
            }
            else LOGE("FAILED TO PRINT STACK TRACE for %p in %s: %s", this,
                      name, strerror(errno));
        }
    }

private:
    struct ref_entry
    {
        ref_entry* next;
        const void* id;
#if DEBUG_REFS_CALLSTACK_ENABLED
        CallStack stack;
#endif
        int32_t ref;
    };

    void addRef(ref_entry** refs, const void* id, int32_t mRef)
    {
        if (mTrackEnabled) {
            AutoMutex _l(mMutex);
            ref_entry* ref = new ref_entry;
            // Reference count at the time of the snapshot, but before the
            // update.  Positive value means we increment, negative--we
            // decrement the reference count.
            ref->ref = mRef;
            ref->id = id;
#if DEBUG_REFS_CALLSTACK_ENABLED
            ref->stack.update(2);
#endif
            
            ref->next = *refs;
            *refs = ref;
        }
    }

    void removeRef(ref_entry** refs, const void* id)
    {
        if (mTrackEnabled) {
            AutoMutex _l(mMutex);
            
            ref_entry* ref = *refs;
            while (ref != NULL) {
                if (ref->id == id) {
                    *refs = ref->next;
                    delete ref;
                    return;
                }
                
                refs = &ref->next;
                ref = *refs;
            }
            
            LOG_ALWAYS_FATAL("RefBase: removing id %p on RefBase %p (weakref_type %p) that doesn't exist!",
                             id, mBase, this);
        }
    }

    void printRefsLocked(String8* out, const ref_entry* refs) const
    {
        char buf[128];
        while (refs) {
            char inc = refs->ref >= 0 ? '+' : '-';
            sprintf(buf, "\t%c ID %p (ref %d):\n", 
                    inc, refs->id, refs->ref);
            out->append(buf);
#if DEBUG_REFS_CALLSTACK_ENABLED
            out->append(refs->stack.toString("\t\t"));
#else
            out->append("\t\t(call stacks disabled)");
#endif
            refs = refs->next;
        }
    }

    Mutex mMutex;
    ref_entry* mStrongRefs;
    ref_entry* mWeakRefs;

    bool mTrackEnabled;
    // Collect stack traces on addref and removeref, instead of deleting the stack references
    // on removeref that match the address ones.
    bool mRetain;

#if 0
    void addRef(KeyedVector<const void*, int32_t>* refs, const void* id)
    {
        AutoMutex _l(mMutex);
        ssize_t i = refs->indexOfKey(id);
        if (i >= 0) {
            ++(refs->editValueAt(i));
        } else {
            i = refs->add(id, 1);
        }
    }

    void removeRef(KeyedVector<const void*, int32_t>* refs, const void* id)
    {
        AutoMutex _l(mMutex);
        ssize_t i = refs->indexOfKey(id);
        LOG_ALWAYS_FATAL_IF(i < 0, "RefBase: removing id %p that doesn't exist!", id);
        if (i >= 0) {
            int32_t val = --(refs->editValueAt(i));
            if (val == 0) {
                refs->removeItemsAt(i);
            }
        }
    }

    void printRefs(const KeyedVector<const void*, int32_t>& refs)
    {
        const size_t N=refs.size();
        for (size_t i=0; i<N; i++) {
            printf("\tID %p: %d remain\n", refs.keyAt(i), refs.valueAt(i));
        }
    }

    mutable Mutex mMutex;
    KeyedVector<const void*, int32_t> mStrongRefs;
    KeyedVector<const void*, int32_t> mWeakRefs;
#endif

#endif
};

// ---------------------------------------------------------------------------

void RefBase::incStrong(const void* id) const
{   /* id 是 sp 自身的指针. */
    weakref_impl* const refs = mRefs;
    refs->addWeakRef(id);
    /*  增加对象的弱引用计数. weakref_impl 继承自 weakref_type 类，
        这里实际调用 weakref_type::incWeak() 来增加对象的弱引用计数的.
    */
    refs->incWeak(id);

    refs->addStrongRef(id);
    /* 增加对象的强引用计数. 返回原来的值.*/
    const int32_t c = android_atomic_inc(&refs->mStrong);
    LOG_ASSERT(c > 0, "incStrong() called on %p after last strong ref", refs);
#if PRINT_REFS
    LOGD("incStrong of %p from %p: cnt=%d\n", this, id, c);
#endif
    if (c != INITIAL_STRONG_VALUE)  {
        return;
    }
    /*  如果对象是第一次被强指针引用，则需要将它调整为1.因为 mStrong 的
        初始值是 INITIAL_STRONG_VALUE.
    */
    android_atomic_add(-INITIAL_STRONG_VALUE, &refs->mStrong);
    /*  然后调用对象的成员函数 onFirstRef 来通知对象，以便对象可以执行一些
        初始化工作.子类需要重载这个函数来实现自己的初始化工作.
    */
    const_cast<RefBase*>(this)->onFirstRef();
}

void RefBase::decStrong(const void* id) const
{
    weakref_impl* const refs = mRefs;
    refs->removeStrongRef(id);
    /* 减少对象的强引用计数,返回原来的值. */
    const int32_t c = android_atomic_dec(&refs->mStrong);
#if PRINT_REFS
    LOGD("decStrong of %p from %p: cnt=%d\n", this, id, c);
#endif
    LOG_ASSERT(c >= 1, "decStrong() called on %p too many times", refs);
    if (c == 1) {  /*  这里表示对象的强引用计数为0. */
        /*  调用 onLastStrongRef 来执行一些业务相关的逻辑.
            同时也要考虑是否需要释放该对象.
         */
        const_cast<RefBase*>(this)->onLastStrongRef(id);
        /* 检查对象的生命周期是否受弱引用计数控制. */
        if ((refs->mFlags&OBJECT_LIFETIME_WEAK) != OBJECT_LIFETIME_WEAK) {
            /*  如果不受弱引用计数控制，则直接释放对象.
                同时会导致 RefBase 类的析构函数被调用.
            */
            delete this; 
        }
    }
    refs->removeWeakRef(id);
    /* 减少对象的弱引用计数的操作,实际执行 weakref_type::decWeak() */
    refs->decWeak(id);
}

void RefBase::forceIncStrong(const void* id) const
{
    weakref_impl* const refs = mRefs;
    refs->addWeakRef(id);
    refs->incWeak(id);
    
    refs->addStrongRef(id);
    const int32_t c = android_atomic_inc(&refs->mStrong);
    LOG_ASSERT(c >= 0, "forceIncStrong called on %p after ref count underflow",
               refs);
#if PRINT_REFS
    LOGD("forceIncStrong of %p from %p: cnt=%d\n", this, id, c);
#endif

    switch (c) {
    case INITIAL_STRONG_VALUE:
        android_atomic_add(-INITIAL_STRONG_VALUE, &refs->mStrong);
        // fall through...
    case 0:
        const_cast<RefBase*>(this)->onFirstRef();
    }
}

int32_t RefBase::getStrongCount() const
{
    return mRefs->mStrong;
}



RefBase* RefBase::weakref_type::refBase() const
{
    return static_cast<const weakref_impl*>(this)->mBase;
}

void RefBase::weakref_type::incWeak(const void* id)
{
    /* this 指针实际指向 mRefs 对象. */
    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    impl->addWeakRef(id);
    /* 增加对象的弱引用计数 mWeak 的值，返回原来的值. */
    const int32_t c = android_atomic_inc(&impl->mWeak);
    LOG_ASSERT(c >= 0, "incWeak called on %p after last weak ref", this);
}

void RefBase::weakref_type::decWeak(const void* id)
{   /* this 指针实际指向 mRefs 对象. */
    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    impl->removeWeakRef(id);
    /* 减少对象的弱引用计数 mWeak 的值，并返回原来的值. */
    const int32_t c = android_atomic_dec(&impl->mWeak);
    LOG_ASSERT(c >= 1, "decWeak called on %p too many times", this);
    if (c != 1) return;
    /*  如果执行到这里，则说明弱引用计数和强指针计数都为 0.
        通过判断生命周期控制方式以及该对象是否被强指针引用过，来释放对象.
    */
    if ((impl->mFlags&OBJECT_LIFETIME_WEAK) != OBJECT_LIFETIME_WEAK) {
        if (impl->mStrong == INITIAL_STRONG_VALUE)
            /*  这里表明对象从来没有被强指针引用过,
                就要释放该对象,否则对象将得不到释放*/
            delete impl->mBase;
        else {
//            LOGV("Freeing refs %p of old RefBase %p\n", this, impl->mBase);
            /*  这里表明对象曾经被强指针引用过,但是已经被释放了.
                释放其内部的引用计数器对象 weakref_impl. 
            */
            delete impl;
        }
    } else {/* 对象的生命周期受弱引用计数控制 */
        /* 调用 onLastWeakRef 来执行一些业务相关的逻辑. */
        impl->mBase->onLastWeakRef(id);
        /* 检查对象的生命周期是否完全不受强引用计数和弱引用计数控制. */
        if ((impl->mFlags&OBJECT_LIFETIME_FOREVER) != OBJECT_LIFETIME_FOREVER) {
            /* 这里表明对象受强引用计数和弱引用计数控制，就要释放对象所占用的内存.*/
            delete impl->mBase;
        }
        /*  如果对象的生命周期完全不受强引用计数和弱引用计数控制时，Android 系统
            所提供的智能指针就退化成一个普通的C++指针，需要手动释放.
        */
    }
}

bool RefBase::weakref_type::attemptIncStrong(const void* id)
{   /*  增加对象的弱引用计数. */
    incWeak(id);
    
    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    
    int32_t curCount = impl->mStrong;
    LOG_ASSERT(curCount >= 0, "attemptIncStrong called on %p after underflow",
               this);
    /* 判断该对象是否被其他强指针引用. */
    while (curCount > 0 && curCount != INITIAL_STRONG_VALUE) {
        /*  第一种情况: 对象已经被其他强指针引用,这时对象一定是存在的. 
            通过原子操作来增加对象的强引用计数,成功返回0,失败返回1.    
        */
        if (android_atomic_cmpxchg(curCount, curCount+1, &impl->mStrong) == 0) {
            break;
        }
        curCount = impl->mStrong;
    }

    if (curCount <= 0 || curCount == INITIAL_STRONG_VALUE) {
        /* 第二种情况: 对象没有被任何强指针引用，对象就不一定存在.*/
        bool allow;
        if (curCount == INITIAL_STRONG_VALUE) {
            /*  对象从来没有被强指针引用过, 如果对象生命周期受弱引用影响或
                onIncStrongAttempted 的返回值为 true，则可以升级为强指针.
            */
            // Attempting to acquire first strong reference...  this is allowed
            // if the object does NOT have a longer lifetime (meaning the
            // implementation doesn't need to see this), or if the implementation
            // allows it to happen.
            allow = (impl->mFlags&OBJECT_LIFETIME_WEAK) != OBJECT_LIFETIME_WEAK
                  || impl->mBase->onIncStrongAttempted(FIRST_INC_STRONG, id);
        } else {
            /*  对象之前被强指针引用过, 并且生命周期受弱引用影响并且
                onIncStrongAttempted 返回值为 true,则可以升级为强指针.
            */
            // Attempting to revive the object...  this is allowed
            // if the object DOES have a longer lifetime (so we can safely
            // call the object with only a weak ref) and the implementation
            // allows it to happen.
            allow = (impl->mFlags&OBJECT_LIFETIME_WEAK) == OBJECT_LIFETIME_WEAK
                  && impl->mBase->onIncStrongAttempted(FIRST_INC_STRONG, id);
        }
        if (!allow) {
            /* 这里说明升级强指针失败,先减少对象的弱引用计数,然后返回 false. */
            decWeak(id);
            return false;
        }
        /* 这里说明升级强指针成功,增加对象的强引用计数 */
        curCount = android_atomic_inc(&impl->mStrong);

        // If the strong reference count has already been incremented by
        // someone else, the implementor of onIncStrongAttempted() is holding
        // an unneeded reference.  So call onLastStrongRef() here to remove it.
        // (No, this is not pretty.)  Note that we MUST NOT do this if we
        // are in fact acquiring the first reference.
        if (curCount > 0 && curCount < INITIAL_STRONG_VALUE) {
            impl->mBase->onLastStrongRef(id);
        }
    }
    
    impl->addWeakRef(id);
    impl->addStrongRef(id);

#if PRINT_REFS
    LOGD("attemptIncStrong of %p from %p: cnt=%d\n", this, id, curCount);
#endif

    if (curCount == INITIAL_STRONG_VALUE) {
        android_atomic_add(-INITIAL_STRONG_VALUE, &impl->mStrong);
        impl->mBase->onFirstRef();
    }
    
    return true;
}

bool RefBase::weakref_type::attemptIncWeak(const void* id)
{
    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    
    int32_t curCount = impl->mWeak;
    LOG_ASSERT(curCount >= 0, "attemptIncWeak called on %p after underflow",
               this);
    while (curCount > 0) {
        if (android_atomic_cmpxchg(curCount, curCount+1, &impl->mWeak) == 0) {
            break;
        }
        curCount = impl->mWeak;
    }

    if (curCount > 0) {
        impl->addWeakRef(id);
    }

    return curCount > 0;
}

int32_t RefBase::weakref_type::getWeakCount() const
{
    return static_cast<const weakref_impl*>(this)->mWeak;
}

void RefBase::weakref_type::printRefs() const
{
    static_cast<const weakref_impl*>(this)->printRefs();
}

void RefBase::weakref_type::trackMe(bool enable, bool retain)
{
    static_cast<const weakref_impl*>(this)->trackMe(enable, retain);
}

RefBase::weakref_type* RefBase::createWeak(const void* id) const
{   /* 增加实际引用对象的弱引用计数. */
    mRefs->incWeak(id);
    return mRefs;
}

RefBase::weakref_type* RefBase::getWeakRefs() const
{
    return mRefs;
}

RefBase::RefBase()
    : mRefs(new weakref_impl(this))
{
//    LOGV("Creating refs %p with RefBase %p\n", mRefs, this);
}

RefBase::~RefBase()
{
//    LOGV("Destroying RefBase %p (refs %p)\n", this, mRefs);
    if (mRefs->mWeak == 0) {
        /* 对象的弱引用计数值为0,则把引用计数对象 mRefs 也一起释放. */
//        LOGV("Freeing refs %p of old RefBase %p\n", mRefs, this);
        delete mRefs;
    }
}

void RefBase::extendObjectLifetime(int32_t mode)
{
    android_atomic_or(mode, &mRefs->mFlags);
}

void RefBase::onFirstRef()
{
}

void RefBase::onLastStrongRef(const void* /*id*/)
{
}

bool RefBase::onIncStrongAttempted(uint32_t flags, const void* id)
{
    return (flags&FIRST_INC_STRONG) ? true : false;
}

void RefBase::onLastWeakRef(const void* /*id*/)
{
}
        
}; // namespace android
