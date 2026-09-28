#include "pch.h"

namespace ff::test::base
{
    // Each test builds a throwaway tree under %TEMP% and deletes it afterward. The directory name
    // includes the test name and the process id so concurrent runs can't collide.
    class scoped_dir
    {
    public:
        scoped_dir(const char* name)
        {
            char temp_dir[MAX_PATH];
            DWORD temp_len = ::GetTempPathA((DWORD)std::size(temp_dir), temp_dir);
            Assert::IsTrue(temp_len > 0 && temp_len < std::size(temp_dir));

            int count = ::_snprintf_s(this->buffer, std::size(this->buffer), _TRUNCATE,
                "%sff_file_enum_%s_%lu", temp_dir, name, ::GetCurrentProcessId());
            Assert::IsTrue(count > 0);

            this->remove_all();
            Assert::IsTrue(::CreateDirectoryA(this->buffer, nullptr) != FALSE);
        }

        ~scoped_dir()
        {
            this->remove_all();
        }

        ff_string_view view() const
        {
            ff_string_view result;
            result.data = this->buffer;
            result.count = ::strlen(this->buffer);
            return result;
        }

        std::string child(const char* relative) const
        {
            std::string path(this->buffer);
            path += "\\";
            path += relative;
            return path;
        }

        void make_dir(const char* relative) const
        {
            Assert::IsTrue(::CreateDirectoryA(this->child(relative).c_str(), nullptr) != FALSE);
        }

        void write(const char* relative, const char* contents) const
        {
            std::string path = this->child(relative);
            HANDLE file = ::CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            Assert::IsTrue(file != INVALID_HANDLE_VALUE);

            size_t size = ::strlen(contents);
            if (size)
            {
                DWORD written = 0;
                Assert::IsTrue(::WriteFile(file, contents, (DWORD)size, &written, nullptr) != FALSE);
            }

            ::CloseHandle(file);
        }

    private:
        static void remove_tree(const std::string& path)
        {
            WIN32_FIND_DATAA found{};
            HANDLE handle = ::FindFirstFileA((path + "\\*").c_str(), &found);
            if (handle != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!::strcmp(found.cFileName, ".") || !::strcmp(found.cFileName, ".."))
                    {
                        continue;
                    }

                    std::string child = path + "\\" + found.cFileName;
                    if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        remove_tree(child);
                    }
                    else
                    {
                        ::DeleteFileA(child.c_str());
                    }
                }
                while (::FindNextFileA(handle, &found));

                ::FindClose(handle);
            }

            ::RemoveDirectoryA(path.c_str());
        }

        void remove_all() const
        {
            remove_tree(std::string(this->buffer));
        }

        char buffer[MAX_PATH]{};
    };

    static bool has_name(ff_file_entry* entries, const char* name)
    {
        for (size_t i = 0; i < ff_array_count(entries); i++)
        {
            if (ff_string_equal(entries[i].name, ff_sz_view(name)))
            {
                return true;
            }
        }

        return false;
    }

    static const ff_file_entry* find_name(ff_file_entry* entries, const char* name)
    {
        for (size_t i = 0; i < ff_array_count(entries); i++)
        {
            if (ff_string_equal(entries[i].name, ff_sz_view(name)))
            {
                return &entries[i];
            }
        }

        return nullptr;
    }

    TEST_CLASS(file_enum_tests)
    {
    public:
        TEST_METHOD(stat_reports_size_and_time)
        {
            scoped_dir dir("stat");
            dir.write("a.txt", "12345");

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const std::string file_path = dir.child("a.txt");
            ff_string_view path = ff_sz_view(file_path.c_str());

            ff_file_info info{};
            bool ok = ff_file_stat(path, &info);

            uint64_t size = info.size;
            uint64_t time = info.write_time;
            bool is_dir = info.directory;

            ff_arena_destroy(&arena);

            Assert::IsTrue(ok);
            Assert::AreEqual((uint64_t)5, size);
            Assert::IsTrue(time != 0, L"write_time should be populated");
            Assert::IsFalse(is_dir);
        }

        TEST_METHOD(stat_identifies_a_directory)
        {
            scoped_dir dir("statdir");

            ff_file_info info{};
            bool ok = ff_file_stat(dir.view(), &info);

            Assert::IsTrue(ok);
            Assert::IsTrue(info.directory);
        }

        TEST_METHOD(stat_fails_for_a_missing_file)
        {
            scoped_dir dir("statmissing");

            ff_file_info info{};
            info.size = 12345;

            bool ok = ff_file_stat(ff_sz_view(dir.child("nope.txt").c_str()), &info);

            Assert::IsFalse(ok);
            Assert::AreEqual((uint64_t)0, info.size, L"info must be cleared on failure");
            Assert::IsFalse(ff_file_exists(ff_sz_view(dir.child("nope.txt").c_str())));
        }

        TEST_METHOD(enumerate_walks_subdirectories_with_relative_names)
        {
            scoped_dir dir("walk");
            dir.write("root.png", "a");
            dir.make_dir("sub");
            dir.write("sub\\inner.png", "bb");
            dir.make_dir("sub\\deep");
            dir.write("sub\\deep\\leaf.txt", "ccc");

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_file_entry* entries = ff_file_enumerate(dir.view(), &arena);

            size_t count = entries ? ff_array_count(entries) : 0;
            bool found_root = entries && has_name(entries, "root.png");
            bool found_inner = entries && has_name(entries, "sub/inner.png");
            bool found_leaf = entries && has_name(entries, "sub/deep/leaf.txt");

            const ff_file_entry* leaf = entries ? find_name(entries, "sub/deep/leaf.txt") : nullptr;
            uint64_t leaf_size = leaf ? leaf->info.size : 0;

            ff_arena_destroy(&arena);

            Assert::IsNotNull(entries);
            Assert::AreEqual((size_t)3, count, L"directories themselves must not be returned");
            Assert::IsTrue(found_root, L"root.png");
            Assert::IsTrue(found_inner, L"sub/inner.png uses forward slashes");
            Assert::IsTrue(found_leaf, L"sub/deep/leaf.txt recurses more than one level");
            Assert::AreEqual((uint64_t)3, leaf_size, L"size travels with the entry");
        }

        TEST_METHOD(enumerate_filters_by_extension_case_insensitively)
        {
            scoped_dir dir("ext");
            dir.write("a.png", "a");
            dir.write("b.PNG", "b");
            dir.write("c.txt", "c");
            dir.write("d.png.bak", "d");
            dir.make_dir("sub");
            dir.write("sub\\e.png", "e");

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_file_entry* entries = ff_file_enumerate_extension(dir.view(), FF_SVL(".png"), &arena);

            size_t count = entries ? ff_array_count(entries) : 0;
            bool found_upper = entries && has_name(entries, "b.PNG");
            bool found_nested = entries && has_name(entries, "sub/e.png");
            bool found_bak = entries && has_name(entries, "d.png.bak");

            ff_arena_destroy(&arena);

            Assert::AreEqual((size_t)3, count);
            Assert::IsTrue(found_upper, L".PNG must match .png");
            Assert::IsTrue(found_nested, L"filtering still recurses");
            Assert::IsFalse(found_bak, L"extension must match at the end only");
        }

        TEST_METHOD(enumerate_returns_empty_array_for_empty_directory)
        {
            scoped_dir dir("empty");

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_file_entry* entries = ff_file_enumerate(dir.view(), &arena);
            size_t count = entries ? ff_array_count(entries) : 0;
            bool null_entries = (entries == nullptr);

            ff_arena_destroy(&arena);

            Assert::IsFalse(null_entries, L"an empty directory is not a failure");
            Assert::AreEqual((size_t)0, count);
        }

        TEST_METHOD(enumerate_fails_for_a_missing_root)
        {
            scoped_dir dir("missingroot");

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_file_entry* entries = ff_file_enumerate(ff_sz_view(dir.child("nope").c_str()), &arena);
            bool null_entries = (entries == nullptr);

            ff_arena_destroy(&arena);

            Assert::IsTrue(null_entries, L"a missing root is distinguishable from an empty one");
        }

        TEST_METHOD(enumerate_rejects_a_file_as_root)
        {
            scoped_dir dir("fileroot");
            dir.write("a.txt", "a");

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_file_entry* entries = ff_file_enumerate(ff_sz_view(dir.child("a.txt").c_str()), &arena);
            bool null_entries = (entries == nullptr);

            ff_arena_destroy(&arena);

            Assert::IsTrue(null_entries);
        }

        // The staleness check the resource system needs is built on exactly this: rewriting a file
        // with different contents has to change what a caller sees.
        TEST_METHOD(rewriting_a_file_changes_its_stat)
        {
            scoped_dir dir("stale");
            dir.write("a.txt", "first");

            const std::string file_path = dir.child("a.txt");
            ff_string_view path = ff_sz_view(file_path.c_str());

            ff_file_info before{};
            Assert::IsTrue(ff_file_stat(path, &before));

            ::Sleep(30);
            dir.write("a.txt", "second entry is longer");

            ff_file_info after{};
            Assert::IsTrue(ff_file_stat(path, &after));

            Assert::IsTrue(after.size != before.size || after.write_time != before.write_time,
                L"a rewritten file must look different");
        }

        TEST_METHOD(enumerate_handles_many_files)
        {
            scoped_dir dir("many");

            for (int i = 0; i < 250; i++)
            {
                char name[64];
                ::_snprintf_s(name, std::size(name), _TRUNCATE, "file_%03d.dat", i);
                dir.write(name, "x");
            }

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_file_entry* entries = ff_file_enumerate(dir.view(), &arena);
            size_t count = entries ? ff_array_count(entries) : 0;
            bool found_last = entries && has_name(entries, "file_249.dat");

            ff_arena_destroy(&arena);

            Assert::AreEqual((size_t)250, count, L"the array must grow past its initial capacity");
            Assert::IsTrue(found_last);
        }
    };
}
