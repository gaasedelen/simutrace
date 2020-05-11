/*
 * Copyright 2014 (C) Karlsruhe Institute of Technology (KIT)
 * Marc Rittinghaus, Thorsten Groeninger
 *
 * Simutrace Base Library (libsimubase) is part of Simutrace.
 *
 * libsimubase is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libsimubase is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libsimubase. If not, see <http://www.gnu.org/licenses/>.
 */
#include "SimuPlatform.h"
#include "SimuBaseTypes.h"

#include "ThreadBase.h"

#include "Exceptions.h"
#include <signal.h>
#include <setjmp.h>

namespace SimuTrace {
namespace System
{

    uint32_t getNumLogicalProcessors()
    {
    #if defined(_WIN32)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);

        return sysinfo.dwNumberOfProcessors;
    #else
        return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
    }

}

#if defined(_WIN32)
#define INVALID_THREAD_ID NULL
#else
#define INVALID_THREAD_ID 0
    void* ThreadStart(void* param);

    void _sigbusHandler(int sig, siginfo_t *siginfo, void *ptr)
    {
        ThreadBase* thread = ThreadBase::getCurrentThread();
        assert(thread != nullptr);

        SignalJumpBuffer* jmp = thread->getSignalJmpBuffer();
        assert(jmp != nullptr);
        assert(jmp->signalret != nullptr);

        ::siglongjmp(jmp->signalret, 1);
    }
#endif

    ThreadBase::ThreadBase() :
        _state(TsIdle),
        _retVal(0),
        _priority(0),
    #if defined(_WIN32)
        _thread(),
    #else
        _signalJmp(nullptr),
    #endif
        _threadId(INVALID_THREAD_ID)
    {

    }

    ThreadBase::~ThreadBase()
    {
        if (_threadId != INVALID_THREAD_ID) {
        #if defined(_WIN32)
        #else
            ::pthread_detach(_threadId);
        #endif

            _threadId = INVALID_THREAD_ID;
        }
    }

    // Current thread object stored in thread local storage (TLS)
    __thread ThreadBase* ThreadBase::_currentThread = nullptr;

#if defined(_WIN32)
#else
    void ThreadBase::_prepareSigBusHandling()
    {
        struct sigaction act;
        memset(&act, 0, sizeof(struct sigaction));
        act.sa_sigaction = _sigbusHandler;
        act.sa_flags = SA_SIGINFO;

        // Install signal handler to catch SIGBUS
        if (::sigaction(SIGBUS, &act, 0) != 0) {
            Throw(PlatformException);
        }
    }
#endif

    void ThreadBase::start()
    {
        ThrowOn(_state != TsIdle, InvalidOperationException);

        _state = TsStarting;

    #if defined(_WIN32)
        _thread = ::CreateThread(nullptr, 0, ThreadStart, this, 0, &_threadId);
        if (!_thread.isValid()) {
            _state = TsIdle;
            Throw(PlatformException);
        }
    #else
        int result = ::pthread_create(&_threadId, nullptr, ThreadStart, this);
        if (result != 0) {
            _state = TsIdle;
            Throw(PlatformException, result);
        }
    #endif

        setPriority(_priority);
    }

    int ThreadBase::adopt()
    {
        ThrowOn(_state != TsIdle, InvalidOperationException);

        _state = TsStarting;

    #if defined(_WIN32)
        _thread = ::GetCurrentThread();
        _threadId = ::GetCurrentThreadId();
    #else
        _threadId = ::pthread_self();
    #endif

        setPriority(_priority);

        ThreadStart(this);

        // Release all association with the current thread to prevent
        // the thread class from closing the thread on destruction.
    #if defined(_WIN32)
        _thread.release();
    #endif
        _threadId = INVALID_THREAD_ID;

        return _retVal;
    }

#if defined(_WIN32)
#else
    void ThreadBase::setSignalJmpBuffer(SignalJumpBuffer* jmp)
    {
        _signalJmp = jmp;
    }

    SignalJumpBuffer* ThreadBase::getSignalJmpBuffer() const
    {
        return _signalJmp;
    }
#endif

    void ThreadBase::stop(bool force)
    {
        ThrowOn(!isRunning() || (force && isExecutingThread()),
                InvalidOperationException);

        if (!force) {
            _state = TsStopping;
        } else {
        #if defined(_WIN32)
        #pragma warning(suppress: 6258) // Warning using TerminateThread
            if (!::TerminateThread(_thread, 0xffffffff)) {
                Throw(PlatformException);
            }
        #else
            int result = ::pthread_cancel(_threadId);
            ThrowOn(result != 0, PlatformException, result);
        #endif

            _state = TsFinished;
            _retVal = -1;
        }
    }

    bool ThreadBase::shouldStop() const
    {
        return (_state == TsStopping);
    }

    void ThreadBase::waitForThread() const
    {
        if (!isRunning() || isExecutingThread()) {
            return;
        }

    #if defined(_WIN32)
        if (::WaitForSingleObject(_thread, INFINITE) == WAIT_FAILED) {
            Throw(PlatformException);
        }
    #else
        void* result;
        ::pthread_join(_threadId, &result);
        ThrowOn(result != 0, PlatformException, *((int*)(&result)));
    #endif
    }

    void ThreadBase::setPriority(int priority)
    {
        if (_threadId == INVALID_THREAD_ID) {
            _priority = priority;
            return;
        }

    #if defined(_WIN32)
        if (!::SetThreadPriority(_thread, priority)) {
            Throw(PlatformException);
        }
    #elif (defined(__MACH__) && defined(__APPLE__))
        // Not implemented. Just do nothing.
    #else
        int result = ::pthread_setschedprio(_threadId, priority);
        ThrowOn(result != 0, PlatformException, result);
    #endif

        _priority = priority;
    }

    int ThreadBase::getPriority() const
    {
        return _priority;
    }

    bool ThreadBase::hasFinished() const
    {
        return (_state == TsFinished);
    }

    bool ThreadBase::isRunning() const
    {
        return ((_state == TsStarting) ||
                (_state == TsRunning) ||
                (_state == TsStopping));
    }

    bool ThreadBase::isExecutingThread() const
    {
    #if defined(_WIN32)
        return (_threadId == ThreadBase::getCurrentThreadId());
    #else
        return (::pthread_equal(_threadId, ::pthread_self()));
    #endif
    }

    unsigned long ThreadBase::getId() const
    {
    #if (defined(__MACH__) && defined(__APPLE__))
        uint64_t tid;
        ::pthread_threadid_np(_threadId, &tid);
        return static_cast<long>(tid);
    #else
        return _threadId;
    #endif
    }

    int ThreadBase::getReturnValue() const
    {
        return _retVal;
    }

    void ThreadBase::sleep(uint32_t ms)
    {
    #if defined(_WIN32)
        ::Sleep(ms);
    #else
        ::usleep(ms * 1000);
    #endif
    }

    unsigned long ThreadBase::getCurrentThreadId()
    {
    #if defined(_WIN32)
        return ::GetCurrentThreadId();
    #elif (defined(__MACH__) && defined(__APPLE__))
        uint64_t tid;
        ::pthread_threadid_np(NULL, &tid);
        return static_cast<long>(tid);
    #else
        return ::pthread_self();
    #endif
    }

    unsigned long ThreadBase::getCurrentSystemThreadId()
    {
    #if defined(_WIN32)
        return getCurrentThreadId();
    #else
        return ::syscall(SYS_gettid);
    #endif
    }

    ThreadBase* ThreadBase::getCurrentThread()
    {
        return _currentThread;
    }

    unsigned long ThreadBase::getCurrentProcessId()
    {
    #if defined(_WIN32)
        return ::GetCurrentProcessId();
    #else
        return ::getpid();
    #endif
    }

    // Thread Start Method

#if defined(_WIN32)
    DWORD WINAPI ThreadStart(LPVOID param)
#else
    void* ThreadStart(void* param)
#endif
    {
        ThreadBase* th;
        ThreadBase* oldTh;

        th = static_cast<ThreadBase*>(param);
        oldTh = th->_currentThread;
        th->_currentThread = th;

    #if defined(_WIN32)
    #else
        th->_prepareSigBusHandling();
    #endif

        th->_state = TsRunning;
        try {
            th->_retVal = th->_run();
        } catch (...) {
            th->_retVal = -1;
        }
        th->_state = TsFinished;

    #if defined(_WIN32)
        DWORD retval = (DWORD)th->_retVal;
    #else
        void* retval = reinterpret_cast<void*>(th->_retVal);
    #endif

        th->_onFinalize();
        th->_currentThread = oldTh;
        return retval;
    }

}