// #include "api/DynamicCommandAPI.h"
#include "api/CommandAPI.h"

#include "api/BaseAPI.h"
#include "api/BlockAPI.h"
#include "api/CommandOriginAPI.h"
#include "api/CommandOutputAPI.h"
#include "api/EntityAPI.h"
#include "api/ItemAPI.h"
#include "api/McAPI.h"
#include "api/PlayerAPI.h"
#include "engine/EngineOwnData.h"
#include "engine/GlobalShareData.h"
#include "engine/LocalShareData.h"
#include "legacyapi/utils/STLHelper.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/ServerInfo.h"
#include "magic_enum.hpp"
#include "main/Configs.h"
#include "mc/_HeaderOutputPredefine.h"
#include "mc/codebuilder/MCRESULT.h"
#include "mc/deps/json/JsonHelpers.h"
#include "mc/enums/CurrentCmdVersion.h"
#include "mc/locale/I18n.h"
#include "mc/server/ServerLevel.h"
#include "mc/server/commands/BlockStateCommandParam.h"
#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandBlockNameResult.h"
#include "mc/server/commands/CommandContext.h"
#include "mc/server/commands/CommandOriginLoader.h"
#include "mc/server/commands/CommandOutputParameter.h"
#include "mc/server/commands/CommandOutputType.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CommandVersion.h"
#include "mc/server/commands/MinecraftCommands.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/world/Minecraft.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/registry/ItemStack.h"
#include "mc/world/level/dimension/Dimension.h"
#include "utils/Utils.h"

#include <filesystem>
#include <string>
#include <vector>

//////////////////// Class Definition ////////////////////

ClassDefine<void> PermissionStaticBuilder  = EnumDefineBuilder<OldCommandPermissionLevel>::build("PermType");
ClassDefine<void> ParamTypeStaticBuilder   = EnumDefineBuilder<DynamicCommand::ParameterType>::build("ParamType");
ClassDefine<void> ParamOptionStaticBuilder = EnumDefineBuilder<CommandParameterOption>::build("ParamOption");

ClassDefine<CommandClass> CommandClassBuilder =
    defineClass<CommandClass>("LLSE_Command")
        .constructor(nullptr)
        .instanceProperty("name", &CommandClass::getName)
        .instanceProperty("registered", &CommandClass::isRegistered)

        .instanceFunction("setEnum", &CommandClass::setEnum)
        .instanceFunction("setAlias", &CommandClass::setAlias)
        //.instanceFunction("newParameter", &CommandClass::newParameter)
        .instanceFunction("mandatory", &CommandClass::mandatory)
        .instanceFunction("optional", &CommandClass::optional)
        .instanceFunction("setSoftEnum", &CommandClass::setSoftEnum)
        .instanceFunction("addSoftEnumValues", &CommandClass::addSoftEnumValues)
        .instanceFunction("removeSoftEnumValues", &CommandClass::removeSoftEnumValues)
        .instanceFunction("getSoftEnumValues", &CommandClass::getSoftEnumValues)
        .instanceFunction("getSoftEnumNames", &CommandClass::getSoftEnumNames)
        .instanceFunction("overload", &CommandClass::addOverload)
        .instanceFunction("setCallback", &CommandClass::setCallback)
        .instanceFunction("setup", &CommandClass::setup)

        .build();

//////////////////// Helper ////////////////////

bool LLSERemoveCmdCallback(script::ScriptEngine* engine) {
    erase_if(localShareData->commandCallbacks, [&engine](auto& data) { return data.second.fromEngine == engine; });
    return true;
}

Local<Value> convertResult(DynamicCommand::Result const& result) {
    if (!result.isSet) return Local<Value>(); // null
    switch (result.type) {
    case DynamicCommand::ParameterType::Bool:
        return Boolean::newBoolean(result.getRaw<bool>());
    case DynamicCommand::ParameterType::Int:
        return Number::newNumber(result.getRaw<int>());
    case DynamicCommand::ParameterType::Float:
        return Number::newNumber(result.getRaw<float>());
    case DynamicCommand::ParameterType::String:
        return String::newString(result.getRaw<std::string>());
    case DynamicCommand::ParameterType::Actor: {
        auto arr = Array::newArray();
        for (auto i : result.get<std::vector<Actor*>>()) {
            arr.add(EntityClass::newEntity(i));
        }
        return arr;
    }
    case DynamicCommand::ParameterType::Player: {
        auto arr = Array::newArray();
        for (auto i : result.get<std::vector<Player*>>()) {
            arr.add(PlayerClass::newPlayer(i));
        }
        return arr;
    }
    case DynamicCommand::ParameterType::BlockPos: {
        auto dim = result.origin->getDimension();
        return IntPos::newPos(result.get<BlockPos>(), dim ? (int)dim->getDimensionId() : -1);
    }
    case DynamicCommand::ParameterType::Vec3: {
        auto dim = result.origin->getDimension();
        return FloatPos::newPos(result.get<Vec3>(), dim ? (int)dim->getDimensionId() : -1);
    }
    case DynamicCommand::ParameterType::Message:
        return String::newString(result.getRaw<CommandMessage>().getMessage(*result.origin));
    case DynamicCommand::ParameterType::RawText:
        return String::newString(result.getRaw<std::string>());
    case DynamicCommand::ParameterType::JsonValue:
        return String::newString(JsonHelpers::serialize(result.getRaw<Json::Value>()));
    case DynamicCommand::ParameterType::Item:
        return ItemClass::newItem(
            new ItemStack(result.getRaw<CommandItem>()
                              .createInstance(1, 1, *new CommandOutput(CommandOutputType::None), true)
                              .value_or(ItemInstance::EMPTY_ITEM)),
            true
        );
    case DynamicCommand::ParameterType::Block:
        return BlockClass::newBlock(
            const_cast<Block*>(result.getRaw<CommandBlockName>().resolveBlock(0).getBlock()),
            &const_cast<BlockPos&>(BlockPos::MIN),
            -1
        );
    case DynamicCommand::ParameterType::Effect:
        return String::newString(result.getRaw<MobEffect const*>()->getResourceName());
    case DynamicCommand::ParameterType::Enum:
        return String::newString(result.getRaw<std::string>());
    case DynamicCommand::ParameterType::SoftEnum:
        return String::newString(result.getRaw<std::string>());
    case DynamicCommand::ParameterType::Command:
        return String::newString(result.getRaw<std::unique_ptr<Command>>()->getCommandName());
    case DynamicCommand::ParameterType::ActorType:
        return String::newString(result.getRaw<ActorDefinitionIdentifier const*>()->getCanonicalName());
    default:
        return Local<Value>(); // null
        break;
    }
}

template <typename T>
std::enable_if_t<std::is_enum_v<T>, T> parseEnum(Local<Value> const& value) {
    if (value.isString()) {
        auto tmp = magic_enum::enum_cast<T>(value.toStr());
        if (!tmp.has_value()) throw std::runtime_error("Unable to parse Enum value");
        return tmp.value();
    } else if (value.isNumber()) {
        return (T)value.toInt();
    }
    throw std::runtime_error("Unable to parse Enum value");
}

//////////////////// MC APIs ////////////////////

Local<Value> McClass::runcmd(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 1)
    CHECK_ARG_TYPE(args[0], ValueKind::kString)
    CommandContext context = CommandContext(
        args[0].asString().toString(),
        std::make_unique<ServerCommandOrigin>(
            ServerCommandOrigin("Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, 0)
        )
    );
    try {
        return Boolean::newBoolean(ll::service::getMinecraft()->getCommands().executeCommand(context, false));
    }
    CATCH("Fail in RunCmd!")
}

Local<Value> McClass::runcmdEx(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 1)
    CHECK_ARG_TYPE(args[0], ValueKind::kString)
    auto origin =
        ServerCommandOrigin("Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, 0);
    auto command = ll::service::getMinecraft()->getCommands().compileCommand(
        args[0].asString().toString(),
        origin,
        (CurrentCmdVersion)CommandVersion::CurrentVersion,
        [](std::string const& err) {}
    );
    CommandOutput output(CommandOutputType::AllOutput);
    std::string   outputStr;
    try {
        if (command) {
            command->run(origin, output);
            for (auto msg : output.getMessages()) {
                outputStr = outputStr.append(I18n::get(msg.getMessageId(), msg.getParams())).append("\n");
            }
            if (output.getMessages().size()) {
                outputStr.pop_back();
            }
            Local<Object> resObj = Object::newObject();
            resObj.set("success", output.getSuccessCount() ? true : false);
            resObj.set("output", outputStr);
            return resObj;
        }
        return {};
    }
    CATCH("Fail in RunCmdEx!")
}

// name, description, permission, flag, alias
Local<Value> McClass::newCommand(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    CHECK_ARG_TYPE(args[1], ValueKind::kString);

    try {
        auto name     = args[0].toStr();
        auto instance = DynamicCommand::getInstance(name);
        if (instance) {
            lse::getSelfPluginInstance().getLogger().info(
                "Dynamic command {} already exists, changes will not be "
                "applied except for setOverload!",
                name
            );
            return CommandClass::newCommand(
                const_cast<std::add_pointer_t<std::remove_cv_t<std::remove_pointer_t<decltype(instance)>>>>(instance)
            );
        }

        auto                   desc       = args[1].toStr();
        CommandPermissionLevel permission = CommandPermissionLevel::Admin;
        CommandFlag            flag       = {(CommandFlagValue)0x80};
        std::string            alias;
        if (args.size() > 2) {
            permission = (CommandPermissionLevel)parseEnum<OldCommandPermissionLevel>(args[2]);
            if (args.size() > 3) {
                CHECK_ARG_TYPE(args[3], ValueKind::kNumber);
                flag = {(CommandFlagValue)args[3].toInt()};
                if (args.size() > 4) {
                    CHECK_ARG_TYPE(args[4], ValueKind::kString);
                    alias = args[4].toStr();
                }
            }
        }
        if (ll::service::getCommandRegistry().has_value()) {
            auto command =
                DynamicCommand::createCommand(ll::service::getCommandRegistry().get(), name, desc, permission, flag);
            if (command) {
                if (!alias.empty()) {
                    command->setAlias(alias);
                }
                return CommandClass::newCommand(std::move(command));
            }
            return Boolean::newBoolean(false);
        }
        lse::getSelfPluginInstance().getLogger().warn(
            "Server have not started yet, please don't use mc.newCommand() before server started."
        );
        return Boolean::newBoolean(false);
    }
    CATCH("Fail in newCommand!")
}

//////////////////// Command APIs ////////////////////

CommandClass::CommandClass(std::unique_ptr<DynamicCommandInstance>&& p)
: ScriptClass(ScriptClass::ConstructFromCpp<CommandClass>{}),
  uptr(std::move(p)),
  ptr(uptr.get()),
  registered(false){};

CommandClass::CommandClass(DynamicCommandInstance* p)
: ScriptClass(ScriptClass::ConstructFromCpp<CommandClass>{}),
  uptr(),
  ptr(p),
  registered(true){};

Local<Object> CommandClass::newCommand(std::unique_ptr<DynamicCommandInstance>&& p) {
    auto newp = new CommandClass(std::move(p));
    return newp->getScriptObject();
}

Local<Object> CommandClass::newCommand(DynamicCommandInstance* p) {
    auto newp = new CommandClass(p);
    return newp->getScriptObject();
}

Local<Value> CommandClass::getName() {
    try {
        return String::newString(get()->getCommandName());
    }
    CATCH("Fail in getCommandName!")
}

Local<Value> CommandClass::setAlias(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 1)
    CHECK_ARG_TYPE(args[0], ValueKind::kString)
    try {
        if (registered) return Boolean::newBoolean(true); // TODO
        return Boolean::newBoolean(get()->setAlias(args[0].toStr()));
    }
    CATCH("Fail in setAlias!")
}

// string, vector<string>
Local<Value> CommandClass::setEnum(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2)
    CHECK_ARG_TYPE(args[0], ValueKind::kString)
    CHECK_ARG_TYPE(args[1], ValueKind::kArray)
    try {
        if (registered) return Local<Value>(); // TODO
        auto enumName = args[0].toStr();
        auto enumArr  = args[1].asArray();
        if (enumArr.size() == 0 || !enumArr.get(0).isString()) return Local<Value>();
        vector<string> enumValues;
        for (int i = 0; i < enumArr.size(); ++i) {
            enumValues.push_back(enumArr.get(i).toStr());
        }
        return String::newString(get()->setEnum(enumName, std::move(enumValues)));
    }
    CATCH("Fail in setEnum!")
}

// name, type, optional, description, identifier, option
// name, type, description, identifier, option
// name, type, optional, description, option
// name, type, description, option
Local<Value> CommandClass::newParameter(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    try {
        if (registered) return Boolean::newBoolean(true); // TODO
        auto                          name        = args[0].toStr();
        DynamicCommand::ParameterType type        = parseEnum<DynamicCommand::ParameterType>(args[1]);
        std::string                   description = "";
        bool                          optional    = false;
        std::string                   identifier  = "";
        size_t                        index       = 2;
        CommandParameterOption        option      = (CommandParameterOption)0;
        if (args.size() > index && args[index].isBoolean()) optional = args[index++].asBoolean().value();
        if (args.size() > index && args[index].isString()) description = args[index++].toStr();
        if (args.size() > index && args[index].isString()) identifier = args[index++].toStr();
        if (args.size() > index && args[index].isNumber()) option = (CommandParameterOption)args[index++].toInt();
        if (index != args.size()) throw std::runtime_error("Error Argument in newParameter");
        return Number::newNumber(
            (int64_t)get()->newParameter(name, type, optional, description, identifier, option).index
        );
    }
    CATCH("Fail in newParameter!")
}

// name, type, description, identifier, option
// name, type, description, option
Local<Value> CommandClass::mandatory(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    try {
        if (registered) return Boolean::newBoolean(true); // TODO
        auto                          name        = args[0].toStr();
        DynamicCommand::ParameterType type        = parseEnum<DynamicCommand::ParameterType>(args[1]);
        std::string                   description = "";
        bool                          optional    = false;
        std::string                   identifier  = "";
        size_t                        index       = 2;
        CommandParameterOption        option      = (CommandParameterOption)0;
        if (args.size() > index && args[index].isString()) description = args[index++].toStr();
        if (args.size() > index && args[index].isString()) identifier = args[index++].toStr();
        if (args.size() > index && args[index].isNumber()) option = (CommandParameterOption)args[index++].toInt();
        if (index != args.size()) throw std::runtime_error("Error Argument in newParameter");
        return Number::newNumber(
            (int64_t)get()->newParameter(name, type, optional, description, identifier, option).index
        );
    }
    CATCH("Fail in newParameter!")
}

// name, type, description, identifier, option
// name, type, description, option
Local<Value> CommandClass::optional(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    try {
        if (registered) return Boolean::newBoolean(true); // TODO
        auto                          name        = args[0].toStr();
        DynamicCommand::ParameterType type        = parseEnum<DynamicCommand::ParameterType>(args[1]);
        std::string                   description = "";
        bool                          optional    = true;
        std::string                   identifier  = "";
        size_t                        index       = 2;
        CommandParameterOption        option      = (CommandParameterOption)0;
        if (args.size() > index && args[index].isString()) description = args[index++].toStr();
        if (args.size() > index && args[index].isString()) identifier = args[index++].toStr();
        if (args.size() > index && args[index].isNumber()) option = (CommandParameterOption)args[index++].toInt();
        if (index != args.size()) throw std::runtime_error("Error Argument in newParameter");
        return Number::newNumber(
            (int64_t)get()->newParameter(name, type, optional, description, identifier, option).index
        );
    }
    CATCH("Fail in newParameter!")
}

// vector<identifier>
// vector<index>
Local<Value> CommandClass::addOverload(const Arguments& args) {
    try {
        if (registered) return Boolean::newBoolean(true); // TODO
        auto command = get();
        if (args.size() == 0)
            return Boolean::newBoolean(command->addOverload(std::vector<DynamicCommandInstance::ParameterIndex>{}));
        if (args[0].isNumber()) {
            std::vector<DynamicCommandInstance::ParameterIndex> params;
            for (int i = 0; i < args.size(); ++i) {
                CHECK_ARG_TYPE(args[i], ValueKind::kNumber);
                params.emplace_back(command, (size_t)args[i].asNumber().toInt64());
            }
            return Boolean::newBoolean(command->addOverload(std::move(params)));
        } else if (args[0].isString()) {
            std::vector<std::string> params;
            for (int i = 0; i < args.size(); ++i) {
                CHECK_ARG_TYPE(args[i], ValueKind::kString);
                params.emplace_back(args[i].toStr());
            }
            return Boolean::newBoolean(command->addOverload(std::move(params)));
        } else if (args[0].isArray()) {
            auto arr = args[0].asArray();
            if (arr.size() == 0)
                return Boolean::newBoolean(command->addOverload(std::vector<DynamicCommandInstance::ParameterIndex>{}));
            if (arr.get(0).isNumber()) {
                std::vector<DynamicCommandInstance::ParameterIndex> params;
                for (int i = 0; i < arr.size(); ++i) {
                    CHECK_ARG_TYPE(arr.get(i), ValueKind::kNumber);
                    params.emplace_back(command, (size_t)arr.get(i).asNumber().toInt64());
                }
                return Boolean::newBoolean(command->addOverload(std::move(params)));
            } else if (arr.get(0).isString()) {
                std::vector<std::string> params;
                for (int i = 0; i < arr.size(); ++i) {
                    CHECK_ARG_TYPE(arr.get(i), ValueKind::kString);
                    params.emplace_back(arr.get(i).toStr());
                }
                return Boolean::newBoolean(command->addOverload(std::move(params)));
            }
        }
        LOG_WRONG_ARG_TYPE();
        return Local<Value>();
    }
    CATCH("Fail in addOverload!")
}

void onExecute(
    DynamicCommand const&                                    command,
    CommandOrigin const&                                     origin,
    CommandOutput&                                           output,
    std::unordered_map<std::string, DynamicCommand::Result>& results
) {
    auto  instance    = command.getInstance();
    auto& commandName = instance->getCommandName();
    if (localShareData->commandCallbacks.find(commandName) == localShareData->commandCallbacks.end()) {
        lse::getSelfPluginInstance().getLogger().warn(
            "Command {} failed to execute, is the plugin unloaded?",
            commandName
        );
        return;
    }
    EngineScope enter(localShareData->commandCallbacks[commandName].fromEngine);
    try {
        Local<Object> args = Object::newObject();
        auto          cmd  = CommandClass::newCommand(const_cast<DynamicCommandInstance*>(instance));
        auto          ori  = CommandOriginClass::newCommandOrigin(&origin);
        auto          outp = CommandOutputClass::newCommandOutput(&output);
        for (auto& [name, param] : results) args.set(name, convertResult(param));
        localShareData->commandCallbacks[commandName].func.get().call({}, cmd, ori, outp, args);
    }
    CATCH_WITHOUT_RETURN("Fail in executing command \"" + commandName + "\"!")
}

// not complete
void onExecute2(
    DynamicCommand const&                                    command,
    CommandOrigin const&                                     origin,
    CommandOutput&                                           output,
    std::unordered_map<std::string, DynamicCommand::Result>& results
) {
    auto  instance    = command.getInstance();
    auto& commandName = instance->getCommandName();
    if (localShareData->commandCallbacks.find(commandName) == localShareData->commandCallbacks.end()) {
        lse::getSelfPluginInstance().getLogger().warn(
            "Command {} failed to execute, is the plugin unloaded?",
            commandName
        );
        return;
    }
    EngineScope enter(localShareData->commandCallbacks[commandName].fromEngine);
    try {
        // auto ctx = CommandContextClass::newCommandContext(&command, &origin,
        // &output, &results); Local<Object> args = Object::newObject(); for (auto&
        // [name, param] : results)
        //     args.set(name, convertResult(param));
        // localShareData->commandCallbacks[commandName].func.get().call({}, ctx,
        // args);
    }
    CATCH_WITHOUT_RETURN("Fail in executing command \"" + commandName + "\"!")
}

// function (command, origin, output, results){}
Local<Value> CommandClass::setCallback(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 1);
    CHECK_ARG_TYPE(args[0], ValueKind::kFunction);
    try {
        auto                    func        = args[0].asFunction();
        DynamicCommandInstance* command     = get();
        auto&                   commandName = command->getCommandName();
        localShareData
            ->commandCallbacks[commandName] = {EngineScope::currentEngine(), 0, script::Global<Function>(func)};
        if (registered) return Boolean::newBoolean(true);
        get()->setCallback(onExecute);
        return Boolean::newBoolean(true);
    }
    CATCH("Fail in setCallback!")
}

// setup(Function<Command, Origin, Output, Map<String, Any>>)
Local<Value> CommandClass::setup(const Arguments& args) {
    try {
        if (args.size() > 0) {
            setCallback(args);
        }
        if (registered) return Boolean::newBoolean(true);
        return Boolean::newBoolean(DynamicCommand::setup(ll::service::getCommandRegistry(), std::move(uptr)));
    }
    CATCH("Fail in setup!")
}

Local<Value> CommandClass::isRegistered() { return Boolean::newBoolean(registered); }

Local<Value> CommandClass::toString(const Arguments& args) {
    try {
        return String::newString(fmt::format("<Command({})>", get()->getCommandName()));
    }
    CATCH("Fail in toString!");
}

Local<Value> CommandClass::setSoftEnum(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    CHECK_ARG_TYPE(args[1], ValueKind::kArray);
    try {
        auto name  = args[0].toStr();
        auto enums = parseStringList(args[1].asArray());
        return String::newString(get()->setSoftEnum(name, std::move(enums)));
    }
    CATCH("Fail in setSoftEnum!");
}

Local<Value> CommandClass::addSoftEnumValues(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    CHECK_ARG_TYPE(args[1], ValueKind::kArray);
    try {
        auto name  = args[0].toStr();
        auto enums = parseStringList(args[1].asArray());
        return Boolean::newBoolean(get()->addSoftEnumValues(name, std::move(enums)));
    }
    CATCH("Fail in addSoftEnumValues!");
}

Local<Value> CommandClass::removeSoftEnumValues(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 2);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    CHECK_ARG_TYPE(args[1], ValueKind::kArray);
    try {
        auto name  = args[0].toStr();
        auto enums = parseStringList(args[1].asArray());
        return Boolean::newBoolean(get()->removeSoftEnumValues(name, std::move(enums)));
    }
    CATCH("Fail in removeSoftEnumValues!");
}

Local<Value> CommandClass::getSoftEnumValues(const Arguments& args) {
    CHECK_ARGS_COUNT(args, 1);
    CHECK_ARG_TYPE(args[0], ValueKind::kString);
    try {
        auto name = args[0].toStr();
        return getStringArray(get()->softEnums[name]);
    }
    CATCH("Fail in getSoftEnumValues");
}

Local<Value> CommandClass::getSoftEnumNames(const Arguments& args) {
    try {
        std::vector<std::string> stringArray;
        for (auto& i : get()->softEnums) {
            stringArray.push_back(i.first);
        }
        return getStringArray(stringArray);
    }
    CATCH("Fail in getSoftEnumNames");
}
