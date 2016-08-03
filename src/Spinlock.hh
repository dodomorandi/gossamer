#ifndef SPINLOCK_HH
#define SPINLOCK_HH

// Spinlocks are hard to port.  Here's the general strategy:
//     - On Mac OSX, the OS provides them.
//     - On Linux, use GCC test and set where possible.
//     - On Windows, the OS only supports spinlocks in the DDK.
//       So use Interlocked*Exchange.
//
// On Intel ISA (i.e. everything so far), we try to use the pause
// instruction in spin loops. This gives a hint to the CPU that it
// shouldn't saturate the bus with cache acquire requests until some
// other CPU issues a write.
//
// Pause is technically only supported in SSE2 or higher. This is
// supported on every x86_64 CPU released to date, but in theory may
// not be guaranteed. Thankfully, it's backwards compatible with IA32,
// since it's the same as rep ; nop.

class Spinlock
{
private:

#if defined(GOSS_MACOSX_X64)
    OSSpinLock mLock;
#elif defined(GOSS_WINDOWS_X64)
    int64_t mLock;
#else
    volatile uint64_t mLock;
#endif

public:
    void lock()
    {
#if defined(GOSS_MACOSX_X64)
        OSSpinLockLock(&mLock);
#elif defined(GOSS_WINDOWS_X64)
        while (_InterlockedCompareExchange64(&mLock, 1, 0) != 0)
        {
            YieldProcessor;
        }
#else
        while (__sync_lock_test_and_set(&mLock, 1))
        {
            do
            {
                _mm_pause();
            } while (mLock);
        }
#endif
    }

    void unlock()
    {
#if defined(GOSS_MACOSX_X64)
        OSSpinLockUnlock(&mLock);
#elif defined(GOSS_WINDOWS_X64)
        _InterlockedExchange64(&mLock, 0);
#else
        __sync_lock_release(&mLock);
#endif
    }

    Spinlock()
    {
#ifdef GOSS_MACOSX_X64
        mLock = OS_SPINLOCK_INIT;
#else
        mLock = 0;
#endif
    }
};

class SpinlockHolder
{
public:
    SpinlockHolder(Spinlock& pLock)
        : mLock(pLock)
    {
        mLock.lock();
    }

    ~SpinlockHolder()
    {
        mLock.unlock();
    }

private:
    Spinlock& mLock;
};

#endif // SPINLOCK_HH
