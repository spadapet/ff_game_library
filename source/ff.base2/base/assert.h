#pragma once

namespace ff
{
    typedef bool(*assert_listener_func)(const char* exp, const char* text, const char* file, unsigned int line);
    ff::assert_listener_func assert_listener(ff::assert_listener_func listener);
}

#ifdef _DEBUG

namespace ff::internal
{
	bool assert_core(const char* exp, const char* text, const char* file, unsigned int line);
}

// The one true assert macro (all others call this)
#define FF_ASSERT_MSG(exp, txt) \
{ \
    if(!(exp) && !ff::internal::assert_core(#exp, txt, __FILE__, __LINE__)) \
    { \
        __debugbreak(); \
    } \
}

#else

#define FF_ASSERT_MSG(exp, txt) ((void)0)

#endif

#define FF_ASSERT(exp) FF_ASSERT_MSG(exp, nullptr)
#define FF_ASSERT_RET(exp) { if(!(exp)) { FF_ASSERT_MSG(false, #exp); return; } }
#define FF_ASSERT_RET_VAL(exp, val) { if(!(exp)) { FF_ASSERT_MSG(false, #exp); return (val); } }
#define FF_ASSERT_MSG_RET(exp, txt) { if(!(exp)) { FF_ASSERT_MSG(false, txt); return; } }
#define FF_ASSERT_MSG_RET_VAL(exp, txt, val) { if(!(exp)) { FF_ASSERT_MSG(false, txt); return (val); } }
#define FF_ASSERT_HR(exp) FF_ASSERT_MSG(SUCCEEDED(exp), nullptr)
#define FF_ASSERT_HR_MSG(exp, txt) FF_ASSERT_MSG(SUCCEEDED(exp), txt)
#define FF_ASSERT_HR_RET(exp) { if(FAILED(exp)) { FF_ASSERT_MSG(false, #exp); return; } }
#define FF_ASSERT_HR_RET_VAL(exp, val) { if(FAILED(exp)) { FF_ASSERT_MSG(false, #exp); return (val); } }
#define FF_ASSERT_HR_MSG_RET(exp, txt) { if(FAILED(exp)) { FF_ASSERT_MSG(false, txt); return; } }
#define FF_ASSERT_HR_MSG_RET_VAL(exp, txt, val) { if(FAILED(exp)) { FF_ASSERT_MSG(false, txt); return (val); } }

#define FF_CHECK_RET(exp) { if(!(exp)) return; }
#define FF_CHECK_RET_VAL(exp, val) { if(!(exp)) return (val); }
#define FF_CHECK_HR_RET(exp) { if(FAILED(exp)) return; }
#define FF_CHECK_HR_RET_VAL(exp, val) { if(FAILED(exp)) return (val); }

#define FF_DEBUG_FAIL() FF_ASSERT(false)
#define FF_DEBUG_FAIL_RET() FF_ASSERT_RET(false)
#define FF_DEBUG_FAIL_RET_VAL(val) FF_ASSERT_RET_VAL(false, val)
#define FF_DEBUG_FAIL_MSG(txt) FF_ASSERT_MSG(false, txt)
#define FF_DEBUG_FAIL_MSG_RET(txt) FF_ASSERT_MSG_RET(false, txt)
#define FF_DEBUG_FAIL_MSG_RET_VAL(txt, val) FF_ASSERT_MSG_RET_VAL(false, txt, val)

#ifdef _DEBUG

#define FF_VERIFY(exp) FF_ASSERT_MSG(exp, nullptr)
#define FF_VERIFY_MSG(exp, txt) FF_ASSERT_MSG(exp, txt)
#define FF_VERIFY_HR(exp) FF_ASSERT_MSG(SUCCEEDED(exp), nullptr)
#define FF_VERIFY_HR_MSG(exp, txt) FF_ASSERT_MSG(SUCCEEDED(exp), txt)

#else

#define FF_VERIFY(exp) (void)(exp)
#define FF_VERIFY_MSG(exp, txt) (void)(exp)
#define FF_VERIFY_HR(exp) (void)(exp)
#define FF_VERIFY_HR_MSG(exp, txt) (void)(exp)

#endif
