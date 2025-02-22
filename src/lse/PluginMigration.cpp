#include "PluginMigration.h"

#include "Entry.h"

#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <ll/api/plugin/Manifest.h>
#include <ll/api/plugin/Plugin.h>
#include <ll/api/reflection/Serialization.h>
#include <nlohmann/json.hpp>
#include <unordered_set>

#if LEGACY_SCRIPT_ENGINE_BACKEND_LUA

constexpr auto PluginExtName = ".lua";

#endif

#ifdef LEGACY_SCRIPT_ENGINE_BACKEND_QUICKJS

constexpr auto PluginExtName = ".js";

#endif

namespace lse {

namespace {

auto migratePlugin(const PluginManager& pluginManager, const std::filesystem::path& path) -> void {
    auto& self = getSelfPluginInstance();

    auto& logger = self.getLogger();

    logger.info("migrating legacy plugin at {}", ll::string_utils::u8str2str(path.u8string()));

    const auto& pluginType = pluginManager.getType();

    const auto& pluginFileName     = path.filename();
    const auto& pluginFileBaseName = path.stem();
    const auto& pluginDir          = ll::plugin::getPluginsRoot() / pluginFileBaseName;

    if (std::filesystem::exists(pluginDir / pluginFileName)) {
        throw std::runtime_error(fmt::format(
            "failed to migrate legacy plugin at {}: {} already exists",
            ll::string_utils::u8str2str(path.u8string()),
            ll::string_utils::u8str2str(pluginDir.u8string())
        ));
    }

    if (!std::filesystem::exists(pluginDir)) {
        if (!std::filesystem::create_directory(pluginDir)) {
            throw std::runtime_error(
                fmt::format("failed to create directory {}", ll::string_utils::u8str2str(pluginDir.u8string()))
            );
        }
    }

    // Move plugin file.
    std::filesystem::rename(path, pluginDir / pluginFileName);

    ll::plugin::Manifest manifest{
        .entry = ll::string_utils::u8str2str(pluginFileName.u8string()),
        .name  = ll::string_utils::u8str2str(pluginFileBaseName.u8string()),
        .type  = pluginType,
        .dependencies =
            std::unordered_set<ll::plugin::Dependency>{
                                                       ll::plugin::Dependency{
                    .name = self.getManifest().name,
                }, },
    };

    auto manifestJson = ll::reflection::serialize<nlohmann::ordered_json>(manifest);

    std::ofstream manifestFile{pluginDir / "manifest.json"};
    manifestFile << manifestJson.dump(4);
}

} // namespace

auto migratePlugins(const PluginManager& pluginManager) -> void {
    auto& self = getSelfPluginInstance();

    auto& logger = self.getLogger();

    std::unordered_set<std::filesystem::path> pluginPaths;

    // Discover plugins.
    logger.info("discovering legacy plugins...");

    const auto& pluginBaseDir = ll::plugin::getPluginsRoot();

    for (const auto& entry : std::filesystem::directory_iterator(pluginBaseDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != PluginExtName) {
            continue;
        }

        pluginPaths.insert(entry.path());
    }

    if (pluginPaths.empty()) {
        logger.info("no legacy plugin found, skipping migration");

        return;
    }

    // Migrate plugins.
    logger.info("migrating legacy plugins...");

    for (const auto& path : pluginPaths) {
        migratePlugin(pluginManager, path);
    }

    logger.warn("legacy plugins have been migrated, please restart the server to load them!");
}

} // namespace lse
