#ifndef ENGINE_CORE_OS_VFS_INL
#define ENGINE_CORE_OS_VFS_INL

#include <mutex>
#include <filesystem>

namespace engine {


    ENGINE_INLINE void VirtualFileSystem::Mount(const std::string& virtual_prefix, const std::string& physical_path) {
        ENGINE_ASSERT(!virtual_prefix.empty(), "VFS mount virtual prefix cannot be empty");
        ENGINE_ASSERT(!physical_path.empty(), "VFS mount physical path cannot be empty");

        std::unique_lock<std::shared_mutex> lock(m_mutex);

        std::string prefix = virtual_prefix;
        if (prefix.length() < 3 || prefix.compare(prefix.length() - 3, 3, "://") != 0) {
            prefix += "://";
        }

        std::string path = physical_path;
        if (!path.empty() && path.back() != '/' && path.back() != '\\') {
            path += "/"; // default to forward slash for engine consistency
        }

        m_mountPoints[prefix] = path;
    }

    ENGINE_INLINE bool VirtualFileSystem::Resolve(const std::string& virtual_path, std::string& out_physical_path) const {
        ENGINE_ASSERT(!virtual_path.empty(), "VFS cannot resolve an empty virtual path");
        ENGINE_ASSERT(virtual_path.length() > 3, "VFS virtual path is too short to contain a scheme");

        std::shared_lock<std::shared_mutex> lock(m_mutex);

        for (const auto& [prefix, physical] : m_mountPoints) {
            if (virtual_path.rfind(prefix, 0) == 0) {
                out_physical_path = physical + virtual_path.substr(prefix.length());
                return true;
            }
        }

        return false;
    }


    ENGINE_INLINE bool VirtualFileSystem::AutoMount(const std::string& virtual_prefix, const std::string& target_folder_name) {
        ENGINE_ASSERT(!virtual_prefix.empty(), "VFS AutoMount virtual prefix cannot be empty");
        ENGINE_ASSERT(!target_folder_name.empty(), "VFS AutoMount target folder name cannot be empty");

        std::filesystem::path current = std::filesystem::current_path();
        for (int i = 0; i < 5; ++i) {
            if (std::filesystem::exists(current / target_folder_name)) {
                Mount(virtual_prefix, (current / target_folder_name).string());
                return true;
            }
            if (current.has_parent_path()) {
                current = current.parent_path();
            } else {
                break;
            }
        }
        return false;
    }

} // namespace engine

#endif // ENGINE_CORE_OS_VFS_INL
