#ifndef ENGINE_CORE_OS_VFS_HPP
#define ENGINE_CORE_OS_VFS_HPP

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include "../OS/types.h"
#include "../debug/assert.h"

namespace engine {


    class VirtualFileSystem {
    public:
        VirtualFileSystem() = default;
        ~VirtualFileSystem() = default;

        // Disable copy semantics for thread safety
        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        ENGINE_INLINE void Mount(const std::string& virtual_prefix, const std::string& physical_path);
        [[nodiscard]] ENGINE_INLINE bool Resolve(const std::string& virtual_path, std::string& out_physical_path) const;
        ENGINE_INLINE bool AutoMount(const std::string& virtual_prefix, const std::string& target_folder_name);

    private:
        mutable std::shared_mutex m_mutex;
        std::unordered_map<std::string, std::string> m_mountPoints;
    };


} // namespace engine

#include "vfs.inl"

#endif // ENGINE_CORE_OS_VFS_HPP
