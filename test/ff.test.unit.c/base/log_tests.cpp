#include "pch.h"

// The log module is process-global, so tests capture output through a free-function sink (a function
// pointer can't be a capturing lambda) and always restore the sink and enabled flags they change.
// This assumes tests run serially, which is the default for the CppUnit framework.
static char captured_text[1024];
static size_t captured_size; // bytes stored in captured_text (capped to its capacity)
static size_t captured_full_size; // full delivered text.size (uncapped)
static int captured_calls;

static void test_sink(ff_log_type type, ff_string_view text)
{
	(void)type;
	captured_calls++;
	captured_full_size = text.count;
	captured_size = (text.count < sizeof(::captured_text)) ? text.count : sizeof(::captured_text);

	if (captured_size)
	{
		::memcpy(::captured_text, text.data, ::captured_size);
	}
}

static void reset_capture()
{
	::captured_calls = 0;
	::captured_size = 0;
	::captured_full_size = 0;
	::captured_text[0] = 0;
}

static size_t cstr_len(const char* text)
{
	size_t len = 0;
	while (text[len])
	{
		len++;
	}

	return len;
}

static bool captured_equals(const char* expected)
{
	size_t len = ::cstr_len(expected);
	return ::captured_size == len && (len == 0 || ::memcmp(::captured_text, expected, len) == 0);
}

static bool sv_equals(ff_string_view view, const char* expected)
{
	size_t len = ::cstr_len(expected);
	return view.count == len && (len == 0 || ::memcmp(view.data, expected, len) == 0);
}

// Exercises ff_log_write_v through a varargs wrapper (mirrors how ff_log_write forwards).
static void call_write_v(ff_log_type type, ff_string_view format, ...)
{
	va_list args;
	va_start(args, format);
	ff_log_write_v(type, format, args);
	va_end(args);
}

namespace ff::test::base
{
	TEST_CLASS(log_tests)
	{
	public:
		// ====================================================================
		// Type table
		// ====================================================================
		TEST_METHOD(type_name_returns_expected)
		{
			Assert::IsTrue(sv_equals(ff_log_get_type_name(ff_log_type_none), "ff"));
			Assert::IsTrue(sv_equals(ff_log_get_type_name(ff_log_type_normal), "ff/app"));
			Assert::IsTrue(sv_equals(ff_log_get_type_name(ff_log_type_debug), "ff/debug"));
		}

		TEST_METHOD(type_enabled_defaults)
		{
			Assert::IsFalse(ff_log_get_type_enabled(ff_log_type_none));
			Assert::IsTrue(ff_log_get_type_enabled(ff_log_type_normal));

#ifdef _DEBUG
			Assert::IsTrue(ff_log_get_type_enabled(ff_log_type_debug));
#else
			Assert::IsFalse(ff_log_get_type_enabled(ff_log_type_debug));
#endif
		}

		TEST_METHOD(type_enabled_set_and_get)
		{
			bool old = ff_log_get_type_enabled(ff_log_type_normal);

			ff_log_set_type_enabled(ff_log_type_normal, false);
			Assert::IsFalse(ff_log_get_type_enabled(ff_log_type_normal));

			ff_log_set_type_enabled(ff_log_type_normal, true);
			Assert::IsTrue(ff_log_get_type_enabled(ff_log_type_normal));

			ff_log_set_type_enabled(ff_log_type_normal, old);
		}

		// ====================================================================
		// Sink install / removal
		// ====================================================================
		TEST_METHOD(sink_install_returns_previous)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);

			// Installing 'prev' returns the sink we just installed, and restores the original.
			Assert::IsTrue(ff_log_set_sink(prev) == &test_sink);
		}

		TEST_METHOD(sink_nullptr_disables_sink)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_normal);
			ff_log_set_type_enabled(ff_log_type_normal, true);

			ff_log_set_sink(nullptr);
			reset_capture();

			ff_log_write(ff_log_type_normal, FF_SVL("no sink"));
			Assert::AreEqual(0, captured_calls);

			ff_log_set_type_enabled(ff_log_type_normal, old);
			ff_log_set_sink(prev);
		}

		// ====================================================================
		// Write formatting
		// ====================================================================
		TEST_METHOD(write_formats_line_with_prefix)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_normal);
			ff_log_set_type_enabled(ff_log_type_normal, true);
			reset_capture();

			ff_log_write(ff_log_type_normal, FF_SVL("hello"));

			Assert::AreEqual(1, captured_calls);
			Assert::IsTrue(captured_equals("[ff/app] hello\r\n"));

			ff_log_set_type_enabled(ff_log_type_normal, old);
			ff_log_set_sink(prev);
		}

		TEST_METHOD(write_uses_type_name_prefix)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_debug);
			ff_log_set_type_enabled(ff_log_type_debug, true);
			reset_capture();

			ff_log_write(ff_log_type_debug, FF_SVL("x"));
			Assert::IsTrue(captured_equals("[ff/debug] x\r\n"));

			ff_log_set_type_enabled(ff_log_type_debug, old);
			ff_log_set_sink(prev);
		}

		TEST_METHOD(write_applies_printf_args)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_normal);
			ff_log_set_type_enabled(ff_log_type_normal, true);
			reset_capture();

			ff_log_write(ff_log_type_normal, FF_SVL("%d-%s"), 42, "x");
			Assert::IsTrue(captured_equals("[ff/app] 42-x\r\n"));

			ff_log_set_type_enabled(ff_log_type_normal, old);
			ff_log_set_sink(prev);
		}

		TEST_METHOD(write_v_formats_like_write)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_normal);
			ff_log_set_type_enabled(ff_log_type_normal, true);
			reset_capture();

			::call_write_v(ff_log_type_normal, FF_SVL("n=%d"), 7);
			Assert::IsTrue(captured_equals("[ff/app] n=7\r\n"));

			ff_log_set_type_enabled(ff_log_type_normal, old);
			ff_log_set_sink(prev);
		}

		TEST_METHOD(write_long_message_is_not_truncated)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_normal);
			ff_log_set_type_enabled(ff_log_type_normal, true);
			reset_capture();

			// Larger than the 1024-byte stack buffer, so the arena spills to the heap; the full line
			// (prefix + 4000 chars + "\r\n") must still be delivered intact.
			ff_log_write(ff_log_type_normal, FF_SVL("%04000d"), 0);

			Assert::AreEqual(1, captured_calls);
			Assert::AreEqual<size_t>(::cstr_len("[ff/app] ") + 4000 + ::cstr_len("\r\n"), captured_full_size);

			ff_log_set_type_enabled(ff_log_type_normal, old);
			ff_log_set_sink(prev);
		}

		// ====================================================================
		// Disabled types are skipped
		// ====================================================================
		TEST_METHOD(write_disabled_type_is_noop)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			bool old = ff_log_get_type_enabled(ff_log_type_normal);
			ff_log_set_type_enabled(ff_log_type_normal, false);
			reset_capture();

			ff_log_write(ff_log_type_normal, FF_SVL("should not appear"));
			Assert::AreEqual(0, captured_calls);

			ff_log_set_type_enabled(ff_log_type_normal, old);
			ff_log_set_sink(prev);
		}

		TEST_METHOD(write_none_type_is_noop_by_default)
		{
			ff_log_sink_func prev = ff_log_set_sink(&test_sink);
			reset_capture();

			// 'none' is disabled by default.
			ff_log_write(ff_log_type_none, FF_SVL("hidden"));
			Assert::AreEqual(0, captured_calls);

			ff_log_set_sink(prev);
		}
	};
}
