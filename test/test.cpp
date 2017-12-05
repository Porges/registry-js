// Required minimum version for RegDeleteTree:
#define _WIN32_WINNT _WIN32_WINNT_VISTA 
#define USING_V8_PLATFORM_SHARED 1

#include "main.hh"

#include "CppUnitLite/TestHarness.h"

#include "libplatform/libplatform.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

/*** Helpers: ***/

std::wstring random_name()
{
    return L"TEST_" + std::to_wstring(std::rand());
}

struct isolate_deleter
{
    void operator()(v8::Isolate* isolate)
    {
        if (isolate)
        {
            isolate->Dispose();
        }
    }
};

using isolate_handle = std::unique_ptr<v8::Isolate, isolate_deleter>;

isolate_handle new_isolate()
{
    auto result = isolate_handle{ v8::Isolate::New(v8::Isolate::CreateParams{}) };
    if (!result)
    {
        throw std::runtime_error("Couldn't create isolate");
    }

    return result;
}

struct reg_key_deleter
{
    using pointer = HKEY;

    void operator()(HKEY h)
    {
        if (h)
        {
            RegCloseKey(h);
        }
    }
};

using unique_reg_key = std::unique_ptr<HKEY, reg_key_deleter>;

class registry_key
{
protected:
    unique_reg_key handle_;

public:
    explicit registry_key(unique_reg_key&& handle) noexcept
        : handle_{ std::move(handle) }
    {}

    HKEY hkey() const noexcept
    {
        return handle_.get();
    }

    static registry_key create_volatile(HKEY root, const std::wstring& subKey)
    {
        HKEY key {};

        auto result =
            RegCreateKeyEx(
                root,
                subKey.c_str(),
                0,
                nullptr,
                REG_OPTION_VOLATILE,
                KEY_ALL_ACCESS,
                nullptr,
                &key,
                nullptr);

        if (result != ERROR_SUCCESS)
        {
            throw std::system_error(result, std::system_category());
        }

        return registry_key{ unique_reg_key{ key } };
    }

    void set_string(const std::wstring& valueName, const std::wstring& value)
    {
        auto result =
            RegSetValueEx(
                handle_.get(),
                valueName.c_str(),
                0 /* reserved */,
                REG_SZ,
                reinterpret_cast<const BYTE*>(value.c_str()),
                (value.size() + 1) * sizeof(wchar_t));

        if (result != ERROR_SUCCESS)
        {
            throw std::system_error(result, std::system_category());
        }
    }

    void set_string_unterminated(const std::wstring& valueName, const std::wstring& value)
    {
        auto result =
            RegSetValueEx(
                handle_.get(),
                valueName.c_str(),
                0 /* reserved */,
                REG_SZ,
                reinterpret_cast<const BYTE*>(value.c_str()),
                value.size() * sizeof(wchar_t) /* whoops! */ );

        if (result != ERROR_SUCCESS)
        {
            throw std::system_error(result, std::system_category());
        }
    }

    void set_value(const std::wstring& valueName, DWORD value)
    {
        auto result =
            RegSetValueEx(
                handle_.get(),
                valueName.c_str(),
                0 /* reserved */,
                REG_DWORD,
                reinterpret_cast<const BYTE*>(&value),
                sizeof(value));

        if (result != ERROR_SUCCESS)
        {
            throw std::system_error(result, std::system_category());
        }
    }
};

class temporary_key : public registry_key
{
    std::wstring name_;

public:
    temporary_key()
        : temporary_key{ random_name() }
    { }

    explicit temporary_key(const std::wstring& name)
        : registry_key{ create_volatile(HKEY_CURRENT_USER, name) }
        , name_{ name }
    { }

    ~temporary_key()
    {
        if (handle_)
        {
            RegDeleteTree(HKEY_CURRENT_USER, name_.c_str());
        }
    }
};

/*** Tests: ***/

TEST( EvilDataTests, Can_read_back_REG_SZ_without_null_terminator )
{
    auto isolate = new_isolate();

    temporary_key key;

    key.set_string(L"goodstring", L"12345");
    key.set_string_unterminated(L"badstring", L"12345");

    auto values = EnumerateValues(key.hkey(), isolate.get());

    //for (auto i = 0; i < values.size(); ++i)
    {

    }
    
}

}

int main()
{
    std::unique_ptr<v8::Platform> platform { v8::platform::CreateDefaultPlatform() };
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    try
    {
        TestResult tr;
        TestRegistry::runAllTests(tr);

        v8::V8::Dispose();
        v8::V8::ShutdownPlatform();
        
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;

        return 1;
    }
}
