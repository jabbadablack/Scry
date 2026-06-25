#ifndef ENGINE_CORE_OS_VFS_HPP
#define ENGINE_CORE_OS_VFS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include "../OS/types.h"
#include "../debug/assert.h"
#include "../ecs/ecs_allocator.hpp"

namespace engine {


    class VirtualFileSystem {
    public:
        struct PakHeader {
            u32 magic       = 0x59524353; // "SCRY"
            u32 version     = 1;
            u32 num_entries = 0;
        };

        struct PakEntry {
            char virtual_path[256];
            u64  offset;
            u64  size;
        };

        VirtualFileSystem() = default;
        ~VirtualFileSystem() = default;

        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        ENGINE_INLINE void Mount(const std::string& virtual_prefix, const std::string& physical_path);
        [[nodiscard]] ENGINE_INLINE bool Resolve(const std::string& virtual_path, std::string& out_physical_path) const;
        ENGINE_INLINE bool AutoMount(const std::string& virtual_prefix, const std::string& target_folder_name);

        // Returns the physical root path registered for a virtual prefix (empty if not mounted)
        [[nodiscard]] ENGINE_INLINE std::string GetMountPath(const std::string& virtual_prefix) const;

        // Serializes an entire physical directory into a single binary .pak file
        ENGINE_INLINE bool PackDirectory(const std::string& virtual_prefix, const std::string& physical_path, const std::string& out_pak_file) const;

        // Mounts a binary .pak file into the VFS
        ENGINE_INLINE bool MountPak(const std::string& pak_path);

        // Reads a file fully into a memory buffer (from either .pak or physical disk)
        ENGINE_INLINE bool ReadFile(const std::string& virtual_path, std::vector<std::byte, engine::ecs::EcsAllocator<std::byte>>& out_buffer) const;

    private:
        mutable std::shared_mutex m_mutex;
        std::unordered_map<std::string, std::string> m_mountPoints;
        std::unordered_map<std::string, PakEntry>    m_pakEntries;
        std::string                                   m_mountedPakPath;
    };


} // namespace engine

#include "vfs.inl"

#endif // ENGINE_CORE_OS_VFS_HPP
