﻿#pragma once

#include "lse/Entry.h"
#include "main/Global.h"
#include "utils/JsonHelper.h"
#include "utils/UsingScriptX.h"

#include <engine/EngineOwnData.h>
#include <exception>
#include <ll/api/utils/ErrorUtils.h>
#include <magic_enum.hpp>
#include <mc/world/level/Level.h>
#include <string>

// 输出异常信息
inline void PrintException(const script::Exception& e) {
    std::ostringstream sout;
    sout << "script::Exception: ";
    sout << e;
    lse::getSelfPluginInstance().getLogger().error(sout.str());
}

inline void PrintScriptStackTrace(std::string const& msg = "") {
    if (!msg.empty()) {
        PrintException(script::Exception(msg));
    } else {
        lse::getSelfPluginInstance().getLogger().error(script::Exception(msg).message());
    }
}

// 方便提取类型
#define toStr() asString().toString()
#define toInt() asNumber().toInt32()

// 实例类类型检查
template <typename T>
bool inline IsInstanceOf(Local<Value> v) {
    return EngineScope::currentEngine()->isInstanceOf<T>(v);
}

std::string ValueKindToString(const ValueKind& kind);

#if !defined(NEW_DEFINES)

// 输出脚本调用堆栈，API名称，以及插件名
#define LOG_ERROR_WITH_SCRIPT_INFO(...)                                                                                \
    PrintScriptStackTrace(__VA_ARGS__);                                                                                \
    lse::getSelfPluginInstance().getLogger().error("In API: " __FUNCTION__);                                           \
    lse::getSelfPluginInstance().getLogger().error("In Plugin: " + ENGINE_OWN_DATA()->pluginName)

// 参数类型错误输出
#define LOG_WRONG_ARG_TYPE() LOG_ERROR_WITH_SCRIPT_INFO("Wrong type of argument!");

// 参数数量错误输出
#define LOG_TOO_FEW_ARGS() LOG_ERROR_WITH_SCRIPT_INFO("Too Few arguments!");

// 参数数量错误输出
#define LOG_WRONG_ARGS_COUNT() LOG_ERROR_WITH_SCRIPT_INFO("Wrong number of arguments!");

// 至少COUNT个参数
#define CHECK_ARGS_COUNT(ARGS, COUNT)                                                                                  \
    if (ARGS.size() < COUNT) {                                                                                         \
        LOG_TOO_FEW_ARGS();                                                                                            \
        return Local<Value>();                                                                                         \
    }

// 检查是否TYPE类型
#define CHECK_ARG_TYPE(ARG, TYPE)                                                                                      \
    if (ARG.getKind() != TYPE) {                                                                                       \
        LOG_WRONG_ARG_TYPE();                                                                                          \
        return Local<Value>();                                                                                         \
    }

// 截获引擎异常
#define CATCH(LOG)                                                                                                     \
    catch (const Exception& e) {                                                                                       \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        PrintException(e);                                                                                             \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
        return Local<Value>();                                                                                         \
    }                                                                                                                  \
    catch (...) {                                                                                                      \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        ll::error_utils::printCurrentException(lse::getSelfPluginInstance().getLogger());                              \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
        return Local<Value>();                                                                                         \
    }

// 至少COUNT个参数_Constructor
#define CHECK_ARGS_COUNT_C(ARGS, COUNT)                                                                                \
    if (ARGS.size() < COUNT) {                                                                                         \
        LOG_TOO_FEW_ARGS();                                                                                            \
        return nullptr;                                                                                                \
    }

// 检查是否TYPE类型_Constructor
#define CHECK_ARG_TYPE_C(ARG, TYPE)                                                                                    \
    if (ARG.getKind() != TYPE) {                                                                                       \
        LOG_WRONG_ARG_TYPE();                                                                                          \
        return nullptr;                                                                                                \
    }

// 检查是否TYPE类型_Setter
#define CHECK_ARG_TYPE_S(ARG, TYPE)                                                                                    \
    if (ARG.getKind() != TYPE) {                                                                                       \
        LOG_WRONG_ARG_TYPE();                                                                                          \
        return;                                                                                                        \
    }

// 截获引擎异常_Constructor
#define CATCH_C(LOG)                                                                                                   \
    catch (const Exception& e) {                                                                                       \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        PrintException(e);                                                                                             \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
        return nullptr;                                                                                                \
    }                                                                                                                  \
    catch (...) {                                                                                                      \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        ll::error_utils::printCurrentException(lse::getSelfPluginInstance().getLogger());                              \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
        return nullptr;                                                                                                \
    }

// 截获引擎异常_Setter
#define CATCH_S(LOG)                                                                                                   \
    catch (const Exception& e) {                                                                                       \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        PrintException(e);                                                                                             \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
    }                                                                                                                  \
    catch (...) {                                                                                                      \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        ll::error_utils::printCurrentException(lse::getSelfPluginInstance().getLogger());                              \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
        return;                                                                                                        \
    }

// 截获引擎异常_Constructor
#define CATCH_WITHOUT_RETURN(LOG)                                                                                      \
    catch (const Exception& e) {                                                                                       \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        PrintException(e);                                                                                             \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
    }                                                                                                                  \
    catch (...) {                                                                                                      \
        lse::getSelfPluginInstance().getLogger().error(LOG);                                                           \
        ll::error_utils::printCurrentException(lse::getSelfPluginInstance().getLogger());                              \
                                                                                                                       \
        LOG_ERROR_WITH_SCRIPT_INFO();                                                                                  \
    }

// 截获回调函数异常
#define CATCH_IN_CALLBACK(callback)                                                                                    \
    catch (const Exception& e) {                                                                                       \
        PrintException(e);                                                                                             \
        lse::getSelfPluginInstance().getLogger().error(std::string("In callback for ") + callback);                    \
        lse::getSelfPluginInstance().getLogger().error("In Plugin: " + ENGINE_OWN_DATA()->pluginName);                 \
    }

#else

// 新的宏定义, 把异常抛入脚本层处理

#define CATCH_AND_THROW                                                                                                \
    catch (const Exception& e) {                                                                                       \
        throw e;                                                                                                       \
    }                                                                                                                  \
    catch (const std::exception& e) {                                                                                  \
        throw Exception(e.what());                                                                                     \
    }                                                                                                                  \
    catch (...) {                                                                                                      \
        throw Exception("Unknown exception in " __FUNCTION__);                                                         \
    }

#define CHECK_ARGS_COUNT(count)                                                                                        \
    if (args.size() != count) {                                                                                        \
        throw Exception(                                                                                               \
            fmt::format("Invalid arguments count: {}, expect {}, in API {}", args.size(), count, __FUNCTION__)         \
        );                                                                                                             \
    }

#define CHECK_ARGS_LEAST_COUNT(count)                                                                                  \
    if (args.size() < count) {                                                                                         \
        throw Exception(fmt::format(                                                                                   \
            "Invalid arguments count: {}, expect at least {}, in API {}",                                              \
            args.size(),                                                                                               \
            count,                                                                                                     \
            __FUNCTION__                                                                                               \
        ));                                                                                                            \
    }

#define CHECK_ARG_TYPE(index, type)                                                                                    \
    if (args[index].getKind() != ValueKind::type) {                                                                    \
        throw Exception(fmt::format(                                                                                   \
            "Wrong type of arguments[{}]: {}, expect {}, in API {}",                                                   \
            index,                                                                                                     \
            ValueKindToString(args[index].getKind()),                                                                  \
            ValueKindToString(ValueKind::type),                                                                        \
            __FUNCTION__                                                                                               \
        ));                                                                                                            \
    }

#define CHECK_VAL_TYPE(val, type)                                                                                      \
    if (val.getKind() != ValueKind::type) {                                                                            \
        throw Exception(fmt::format(                                                                                   \
            "Wrong type of value: {}, expect {}, in API {}",                                                           \
            ValueKindToString(val.getKind()),                                                                          \
            ValueKindToString(ValueKind::type),                                                                        \
            __FUNCTION__                                                                                               \
        ));                                                                                                            \
    }

#endif

// 判断是否为浮点数
bool CheckIsFloat(const Local<Value>& num);

// 序列化
template <typename T>
void PrintValue(T& out, Local<Value> v);

std::string ValueToString(Local<Value> v);

// Json 序列化 反序列化
Local<Value> JsonToValue(std::string jsonStr);
Local<Value> JsonToValue(ordered_json j);
std::string  ValueToJson(Local<Value> v, int formatIndent = -1);

// Get the enum's ClassDefine<void> object
// Limitation: enum values must be in range of [-128, 128)
template <typename Type>
struct EnumDefineBuilder {
    template <Type val>
    inline static Local<Value> serialize() {
        return Number::newNumber(static_cast<int>(val));
    }

    inline static Local<Value> keys() {
        try {
            auto arr = Array::newArray();
            for (auto& name : magic_enum::enum_names<Type>()) {
                arr.add(String::newString(name));
            }
            return arr;
        } catch (const std::exception&) {
            lse::getSelfPluginInstance().getLogger().error("Error in " __FUNCTION__);
        }
        return Local<Value>();
    }

    inline static Local<Value> toObject() {
        try {
            auto obj = Object::newObject();
            for (auto& [value, name] : magic_enum::enum_entries<Type>()) {
                obj.set(String::newString(name), Number::newNumber((int)value));
            }
            return obj;
        } catch (const std::exception&) {
            lse::getSelfPluginInstance().getLogger().error("Error in " __FUNCTION__);
        }
        return Local<Value>();
    }

    inline static Local<Value> getName(const Arguments& args) {
        try {
            if (args.size() < 1) return Local<Value>();
            if (args[0].isString())
                return magic_enum::enum_cast<Type>(args[0].toStr()).has_value() ? args[0] : Local<Value>();
            if (args[0].isNumber()) return String::newString(magic_enum::enum_name(static_cast<Type>(args[0].toInt())));
            return Local<Value>();
        } catch (const std::exception&) {
            lse::getSelfPluginInstance().getLogger().error("Error in " __FUNCTION__);
        }
        return Local<Value>();
    }

    inline static Local<Value> toString() {
        try {
            return String::newString(typeid(Type).name() + 5);
        } catch (const std::exception&) {
            lse::getSelfPluginInstance().getLogger().error("Error in " __FUNCTION__);
        }
        return Local<Value>();
    }

    template <
        Type                                         val,
        std::enable_if_t<std::is_enum_v<Type>, char> max = static_cast<char>(*magic_enum::enum_values<Type>().rbegin())>
    inline static void buildBuilder(script::ClassDefineBuilder<void>& builder) {
        if constexpr (static_cast<char>(val) > max) return;
        if constexpr (!magic_enum::enum_name(val).empty()) {
            fmt::print("{} = {},\n", magic_enum::enum_name(val), static_cast<int>(val));
            builder.property(magic_enum::enum_name(val).data(), &serialize<val>);
        }
        buildBuilder<static_cast<Type>((static_cast<char>(val) + 1)), max>(builder);
    }

    template <
        std::enable_if_t<std::is_enum_v<Type>, char> max = static_cast<char>(*magic_enum::enum_values<Type>().rbegin())>
    inline static ClassDefine<void> build(std::string const& enumName) {
        script::ClassDefineBuilder<void> builder = defineClass(enumName);
        // fmt::print("枚举 {} 可能取值：\n", enumName);
        // buildBuilder<*magic_enum::enum_values<Type>().begin(), max>(builder);

        for (auto& [val, name] : magic_enum::enum_entries<Type>()) {
            // fmt::print("{} = {},\n", name, static_cast<int>(val));
            auto _val  = val;
            auto _name = name;
            builder.property(std::string(name), [=]() -> Local<Value> {
                try {
                    return Number::newNumber(static_cast<int>(_val));
                } catch (const std::exception&) {
                    lse::getSelfPluginInstance().getLogger().error("Error in get {}.{}", enumName, _name);
                }
                return Local<Value>();
            });
        }

        builder.property("keys", &keys);
        // builder.property("object", &toObject);
        builder.function("getName", &getName);
        // fmt::print("\n");
        return builder.build();
    }
};

inline void ReplaceStr(std::string& str, const std::string& from, const std::string& to) {
    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos) {
        str.replace(startPos, from.length(), to);
        startPos += to.length();
    }
}
