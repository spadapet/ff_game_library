#include "pch.h"

// The log module is process-global, so tests capture output through a free-function sink (a function
// pointer can't be a capturing lambda) and always restore the sink and enabled flags they change.
// This assumes tests run serially, which is the default for the CppUnit framework.
static char captured_text[1024];
static size_t captured_size; // bytes stored in captured_text (capped to its capacity)
static size_t captured_full_size; // full delivered text.size (uncapped)
static int captured_calls;

static void test_sink(ff::string_view text)
{
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

static bool sv_equals(ff::string_view view, const char* expected)
{
	size_t len = ::cstr_len(expected);
	return view.count == len && (len == 0 || ::memcmp(view.data, expected, len) == 0);
}

// Exercises ff::log::write_v through a varargs wrapper (mirrors how ff::log::write forwards).
static void call_write_v(ff::log::type type, ff::string_view format, ...)
{
	va_list args;
	va_start(args, format);
	ff::log::write_v(type, format, args);
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
			Assert::IsTrue(sv_equals(ff::log::type_name(ff::log::type::none), "ff"));
			Assert::IsTrue(sv_equals(ff::log::type_name(ff::log::type::normal), "ff/game"));
			Assert::IsTrue(sv_equals(ff::log::type_name(ff::log::type::debug), "ff/debug"));
		}

		TEST_METHOD(type_enabled_defaults)
		{
			Assert::IsFalse(ff::log::type_enabled(ff::log::type::none));
			Assert::IsTrue(ff::log::type_enabled(ff::log::type::normal));

#ifdef _DEBUG
			Assert::IsTrue(ff::log::type_enabled(ff::log::type::debug));
#else
			Assert::IsFalse(ff::log::type_enabled(ff::log::type::debug));
#endif
		}

		TEST_METHOD(type_enabled_set_and_get)
		{
			bool old = ff::log::type_enabled(ff::log::type::normal);

			ff::log::type_enabled(ff::log::type::normal, false);
			Assert::IsFalse(ff::log::type_enabled(ff::log::type::normal));

			ff::log::type_enabled(ff::log::type::normal, true);
			Assert::IsTrue(ff::log::type_enabled(ff::log::type::normal));

			ff::log::type_enabled(ff::log::type::normal, old);
		}

		// ====================================================================
		// Sink install / removal
		// ====================================================================
		TEST_METHOD(sink_install_returns_previous)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);

			// Installing 'prev' returns the sink we just installed, and restores the original.
			Assert::IsTrue(ff::log::sink(prev) == &test_sink);
		}

		TEST_METHOD(sink_nullptr_disables_sink)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::normal);
			ff::log::type_enabled(ff::log::type::normal, true);

			ff::log::sink(nullptr);
			reset_capture();

			ff::log::write(ff::log::type::normal, FF_SVL("no sink"));
			Assert::AreEqual(0, captured_calls);

			ff::log::type_enabled(ff::log::type::normal, old);
			ff::log::sink(prev);
		}

		// ====================================================================
		// Write formatting
		// ====================================================================
		TEST_METHOD(write_formats_line_with_prefix)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::normal);
			ff::log::type_enabled(ff::log::type::normal, true);
			reset_capture();

			ff::log::write(ff::log::type::normal, FF_SVL("hello"));

			Assert::AreEqual(1, captured_calls);
			Assert::IsTrue(captured_equals("[ff/game] hello\r\n"));

			ff::log::type_enabled(ff::log::type::normal, old);
			ff::log::sink(prev);
		}

		TEST_METHOD(write_uses_type_name_prefix)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::debug);
			ff::log::type_enabled(ff::log::type::debug, true);
			reset_capture();

			ff::log::write(ff::log::type::debug, FF_SVL("x"));
			Assert::IsTrue(captured_equals("[ff/debug] x\r\n"));

			ff::log::type_enabled(ff::log::type::debug, old);
			ff::log::sink(prev);
		}

		TEST_METHOD(write_applies_printf_args)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::normal);
			ff::log::type_enabled(ff::log::type::normal, true);
			reset_capture();

			ff::log::write(ff::log::type::normal, FF_SVL("%d-%s"), 42, "x");
			Assert::IsTrue(captured_equals("[ff/game] 42-x\r\n"));

			ff::log::type_enabled(ff::log::type::normal, old);
			ff::log::sink(prev);
		}

		TEST_METHOD(write_v_formats_like_write)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::normal);
			ff::log::type_enabled(ff::log::type::normal, true);
			reset_capture();

			::call_write_v(ff::log::type::normal, FF_SVL("n=%d"), 7);
			Assert::IsTrue(captured_equals("[ff/game] n=7\r\n"));

			ff::log::type_enabled(ff::log::type::normal, old);
			ff::log::sink(prev);
		}

		TEST_METHOD(write_long_message_is_not_truncated)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::normal);
			ff::log::type_enabled(ff::log::type::normal, true);
			reset_capture();

			// Larger than the 1024-byte stack buffer, so the arena spills to the heap; the full line
			// (prefix + 4000 chars + "\r\n") must still be delivered intact.
			ff::log::write(ff::log::type::normal, FF_SVL("%04000d"), 0);

			Assert::AreEqual(1, captured_calls);
			Assert::AreEqual<size_t>(::cstr_len("[ff/game] ") + 4000 + ::cstr_len("\r\n"), captured_full_size);

			ff::log::type_enabled(ff::log::type::normal, old);
			ff::log::sink(prev);
		}

		// ====================================================================
		// Disabled types are skipped
		// ====================================================================
		TEST_METHOD(write_disabled_type_is_noop)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			bool old = ff::log::type_enabled(ff::log::type::normal);
			ff::log::type_enabled(ff::log::type::normal, false);
			reset_capture();

			ff::log::write(ff::log::type::normal, FF_SVL("should not appear"));
			Assert::AreEqual(0, captured_calls);

			ff::log::type_enabled(ff::log::type::normal, old);
			ff::log::sink(prev);
		}

		TEST_METHOD(write_none_type_is_noop_by_default)
		{
			ff::log::sink_func prev = ff::log::sink(&test_sink);
			reset_capture();

			// 'none' is disabled by default.
			ff::log::write(ff::log::type::none, FF_SVL("hidden"));
			Assert::AreEqual(0, captured_calls);

			ff::log::sink(prev);
		}
	};
}
