#include "main.hh"

#include <cstdio>
#include <memory>

using namespace Nan;
using namespace v8;

namespace {

const DWORD MAX_VALUE_NAME = 16383;

v8::Local<v8::Object> CreateEntry(Isolate *isolate, LPWSTR name, LPWSTR type, LPWSTR data, DWORD dataLengthBytes)
{
  // NB: We must verify the data, since there's no guarantee that REG_SZ are stored with null terminators.

  // Test is ">= sizeof(wchar_t)" because otherwise 1/2 - 1 = -1 and things go kabloom:
  if (dataLengthBytes >= sizeof(wchar_t) && data[dataLengthBytes/sizeof(wchar_t) - 1] == L'\0')
  {
    // The string is (correctly) null-terminated.
    // Trim off the null terminator before handing it to NewFromTwoByte:
    dataLengthBytes -= sizeof(wchar_t);
  }

  // ... otherwise, it's not null-terminated, but we're passing the explicit length
  // to NewFromTwoByte anyway so we'll be fine (we won't over-read).

  auto v8NameString = v8::String::NewFromTwoByte(isolate, (uint16_t*)name, NewStringType::kNormal);
  auto v8TypeString = v8::String::NewFromTwoByte(isolate, (uint16_t*)type, NewStringType::kNormal);
  auto v8DataString = v8::String::NewFromTwoByte(isolate, (uint16_t*)data, NewStringType::kNormal, dataLengthBytes/sizeof(wchar_t));

  auto obj = Nan::New<v8::Object>();
  obj->Set(Nan::New("name").ToLocalChecked(), v8NameString.ToLocalChecked());
  obj->Set(Nan::New("type").ToLocalChecked(), v8TypeString.ToLocalChecked());
  obj->Set(Nan::New("data").ToLocalChecked(), v8DataString.ToLocalChecked());
  return obj;
}

v8::Local<v8::Object> CreateEntry(Isolate *isolate, LPWSTR name, LPWSTR type, DWORD data)
{
  auto v8NameString = v8::String::NewFromTwoByte(isolate, (uint16_t*)name, NewStringType::kNormal);
  auto v8TypeString = v8::String::NewFromTwoByte(isolate, (uint16_t*)type, NewStringType::kNormal);

  auto obj = Nan::New<v8::Object>();
  obj->Set(Nan::New("name").ToLocalChecked(), v8NameString.ToLocalChecked());
  obj->Set(Nan::New("type").ToLocalChecked(), v8TypeString.ToLocalChecked());
  obj->Set(Nan::New("data").ToLocalChecked(), Nan::New(static_cast<uint32_t>(data)));
  return obj;
}

}

v8::Local<v8::Array> EnumerateValues(HKEY hCurrentKey, Isolate *isolate) {
  DWORD cValues, cchMaxValue, cbMaxValueData;

  auto retCode = RegQueryInfoKey(
    hCurrentKey,
    nullptr, // classname (not needed)
    nullptr, // classname length (not needed)
    nullptr, // reserved
    nullptr, // can ignore subkey values
    nullptr,
    nullptr,
    &cValues, // number of values for key
    &cchMaxValue, // longest value name
    &cbMaxValueData, // longest value data
    nullptr, // can ignore these values
    nullptr);

  if (retCode != ERROR_SUCCESS)
  {
    char errorMessage[49]; // 38 for message + 10 for int + 1 for nul
    sprintf_s(errorMessage, "RegQueryInfoKey failed - exit code: '%d'", retCode);
    Nan::ThrowError(errorMessage);
    return New<v8::Array>(0);
  }

  auto results = New<v8::Array>(cValues);

  auto buffer = std::make_unique<BYTE[]>(cbMaxValueData);
  for (DWORD i = 0; i < cValues; i++)
  {
    auto cchValue = MAX_VALUE_NAME;
    WCHAR achValue[MAX_VALUE_NAME];
    achValue[0] = '\0';

    DWORD lpType;
    DWORD cbData = cbMaxValueData;

    auto retCode = RegEnumValue(
      hCurrentKey,
      i,
      achValue,
      &cchValue,
      nullptr,
      &lpType,
      buffer.get(),
      &cbData);

    if (retCode == ERROR_SUCCESS)
    {
      if (lpType == REG_SZ)
      {
        auto text = reinterpret_cast<LPWSTR>(buffer.get());
        auto obj = CreateEntry(isolate, achValue, L"REG_SZ", text, cbData);
        Nan::Set(results, i, obj);
      }
      else if (lpType == REG_EXPAND_SZ)
      {
        auto text = reinterpret_cast<LPWSTR>(buffer.get());
        auto obj = CreateEntry(isolate, achValue, L"REG_EXPAND_SZ", text, cbData);
        Nan::Set(results, i, obj);
      }
      else if (lpType == REG_DWORD)
      {
        assert(cbData == sizeof(DWORD));
        Nan::Set(results, i, CreateEntry(isolate, achValue, L"REG_DWORD", *reinterpret_cast<DWORD*>(buffer.get())));
      }
    }
    else if (retCode == ERROR_NO_MORE_ITEMS)
    {
      // no more items found, time to wrap up
      break;
    }
    else
    {
      char errorMessage[50]; // 39 for message + 10 for int  + 1 for nul
      sprintf_s(errorMessage, "RegEnumValue returned an error code: '%d'", retCode);
      Nan::ThrowError(errorMessage);
      return New<v8::Array>(0);
    }
  }

  return results;
}

namespace {

NAN_METHOD(ReadValues)
{
  if (info.Length() < 2)
  {
    Nan::ThrowTypeError("Wrong number of arguments");
    return;
  }

  if (!info[0]->IsNumber())
  {
    Nan::ThrowTypeError("A number was expected for the first argument, but wasn't received.");
    return;
  }

  if (!info[1]->IsString())
  {
    Nan::ThrowTypeError("A string was expected for the second argument, but wasn't received.");
    return;
  }

  auto first = reinterpret_cast<HKEY>(info[0]->IntegerValue());
  auto second = *v8::String::Value(info[1]);

  HKEY hCurrentKey;
  LONG openKey = RegOpenKeyEx(
    first,
    (LPWSTR)second,
    0,
    KEY_READ | KEY_WOW64_64KEY,
    &hCurrentKey);

  if (openKey == ERROR_FILE_NOT_FOUND)
  {
    // the key does not exist, just return an empty array for now
    info.GetReturnValue().Set(New<v8::Array>(0));
  }
  else if (openKey == ERROR_SUCCESS)
  {
    v8::Local<v8::Array> results = EnumerateValues(hCurrentKey, info.GetIsolate());
    info.GetReturnValue().Set(results);
    RegCloseKey(hCurrentKey);
  }
  else
  {
    char errorMessage[46]; // 35 for message + 10 for int + 1 for nul
    sprintf_s(errorMessage, "RegOpenKeyEx failed - exit code: '%d'", openKey);
    Nan::ThrowError(errorMessage);
  }
}

void Init(v8::Handle<v8::Object> exports) {
  Nan::SetMethod(exports, "readValues", ReadValues);
}

}

NODE_MODULE(registryNativeModule, Init);
