#pragma once

#ifdef assert
#undef assert
#endif

#ifdef verify
#undef verify
#endif

#ifdef _DEBUG

namespace ff::internal
{
	bool assert_core(const char* exp, const char* text, const char* file, unsigned int line);
    void assert_listener(std::function<bool(const char*, const char*, const char*, unsigned int)>&& listener);
}

// The one true assert macro (all others call this)
#define assert_msg(exp, txt) \
{ \
	if(!(exp) && !ff::internal::assert_core(#exp, txt, __FILE__, __LINE__)) \
	{ \
		__debugbreak(); \
	} \
}

#else // !_DEBUG

#define assert_msg(exp, txt) ((void)0)

#endif

#define assert(exp) assert_msg(exp, nullptr)
#define assert_ret(exp) { if(!(exp)) { assert_msg(false, TEXT(#exp)); return; } }
#define assert_ret_val(exp, val) { if(!(exp)) { assert_msg(false, TEXT(#exp)); return (val); } }
#define assert_msg_ret(exp, txt) { if(!(exp)) { assert_msg(false, txt); return; } }
#define assert_msg_ret_val(exp, txt, val) { if(!(exp)) { assert_msg(false, txt); return (val); } }
#define assert_hr(exp) assert_msg(SUCCEEDED(exp), nullptr)
#define assert_hr_ret(exp) { if(FAILED(exp)) { assert_msg(false, TEXT(#exp)); return; } }
#define assert_hr_ret_val(exp, val) { if(FAILED(exp)) { assert_msg(false, TEXT(#exp)); return (val); } }
#define assert_hr_msg_ret(exp, txt) { if(FAILED(exp)) { assert_msg(false, txt); return; } }
#define assert_hr_msg_ret_val(exp, txt, val) { if(FAILED(exp)) { assert_msg(false, txt); return (val); } }

#define check_ret(exp) { if(!(exp)) return; }
#define check_ret_val(exp, val) { if(!(exp)) return (val); }
#define check_hr_ret(exp) { if(FAILED(exp)) return; }
#define check_hr_ret_val(exp, val) { if(FAILED(exp)) return (val); }

#define fail() assert_msg(false, nullptr);
#define fail_msg(txt) assert_msg(false, txt);

#ifdef _DEBUG

#define verify(exp) assert_msg(exp, nullptr)
#define verify_msg(exp, txt) assert_msg(exp, txt)
#define verify_hr(exp) assert_msg(SUCCEEDED(exp), nullptr)

#else

#define verify(exp) (void)(exp)
#define verify_msg(exp, txt) (void)(exp)
#define verify_hr(exp) (void)(exp)

#endif
