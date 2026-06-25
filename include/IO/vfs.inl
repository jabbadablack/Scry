#ifndef ENGINE_CORE_OS_VFS_INL
#define ENGINE_CORE_OS_VFS_INL

#include <mutex>
#include <filesystem>
#include <fstream>
#include <cstring>

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
            path += "/";
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

    ENGINE_INLINE std::string VirtualFileSystem::GetMountPath(const std::string& virtual_prefix) const {
        std::string prefix = virtual_prefix;
        if (prefix.length() < 3 || prefix.compare(prefix.length() - 3, 3, "://") != 0) {
            prefix += "://";
        }
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        const auto it = m_mountPoints.find(prefix);
        return it != m_mountPoints.end() ? it->second : std::string{};
    }

    ENGINE_INLINE bool VirtualFileSystem::PackDirectory(const std::string& virtual_prefix, const std::string& physical_path, const std::string& out_pak_file) const {
        namespace fs = std::filesystem;

        if (!fs::exists(physical_path) || !fs::is_directory(physical_path)) {
            return false;
        }

        // Normalise prefix to have "://"
        std::string prefix = virtual_prefix;
        if (prefix.length() < 3 || prefix.compare(prefix.length() - 3, 3, "://") != 0) {
            prefix += "://";
        }

        // First pass: collect files
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(physical_path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }

        // Build entry table with pre-calculated offsets
        const fs::path base_path(physical_path);
        std::vector<PakEntry> entries(files.size());
        u64 data_offset = sizeof(PakHeader) + static_cast<u64>(files.size()) * sizeof(PakEntry);

        for (size_t i = 0; i < files.size(); ++i) {
            auto rel = files[i].lexically_relative(base_path);
            std::string vpath = prefix + rel.generic_string();
            std::strncpy(entries[i].virtual_path, vpath.c_str(), sizeof(PakEntry::virtual_path) - 1);
            entries[i].virtual_path[sizeof(PakEntry::virtual_path) - 1] = '\0';
            entries[i].size   = static_cast<u64>(fs::file_size(files[i]));
            entries[i].offset = data_offset;
            data_offset += entries[i].size;
        }

        // Write pak
        std::ofstream out(out_pak_file, std::ios::binary);
        if (!out) {
            return false;
        }

        PakHeader header;
        header.num_entries = static_cast<u32>(files.size());
        out.write(reinterpret_cast<const char*>(&header), sizeof(PakHeader));
        out.write(reinterpret_cast<const char*>(entries.data()), static_cast<std::streamsize>(files.size() * sizeof(PakEntry)));

        for (size_t i = 0; i < files.size(); ++i) {
            std::ifstream in(files[i], std::ios::binary);
            if (!in) {
                return false;
            }
            out << in.rdbuf();
        }

        return out.good();
    }

    ENGINE_INLINE bool VirtualFileSystem::MountPak(const std::string& pak_path) {
        std::ifstream in(pak_path, std::ios::binary);
        if (!in) {
            return false;
        }

        PakHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(PakHeader));
        if (!in || header.magic != 0x59524353u) {
            return false;
        }

        std::vector<PakEntry> entries(header.num_entries);
        in.read(reinterpret_cast<char*>(entries.data()),
                static_cast<std::streamsize>(header.num_entries * sizeof(PakEntry)));
        if (!in) {
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_pakEntries.clear();
        for (const auto& entry : entries) {
            m_pakEntries[entry.virtual_path] = entry;
        }
        m_mountedPakPath = pak_path;
        return true;
    }

    ENGINE_INLINE bool VirtualFileSystem::ReadFile(const std::string& virtual_path, std::vector<std::byte, engine::ecs::EcsAllocator<std::byte>>& out_buffer) const {
        // 1. Pak lookup (shared lock, released before fallback)
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_pakEntries.find(virtual_path);
            if (it != m_pakEntries.end()) {
                const PakEntry& entry = it->second;
                std::ifstream in(m_mountedPakPath, std::ios::binary);
                if (!in) {
                    return false;
                }
                in.seekg(static_cast<std::streamoff>(entry.offset));
                out_buffer.resize(static_cast<size_t>(entry.size));
                in.read(reinterpret_cast<char*>(out_buffer.data()), static_cast<std::streamsize>(entry.size));
                return static_cast<u64>(in.gcount()) == entry.size;
            }
        }

        // 2. Physical fallback via mount points
        std::string physical_path;
        if (!Resolve(virtual_path, physical_path)) {
            return false;
        }

        std::ifstream in(physical_path, std::ios::binary);
        if (!in) {
            return false;
        }

        in.seekg(0, std::ios::end);
        const auto file_size = static_cast<size_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        out_buffer.resize(file_size);
        in.read(reinterpret_cast<char*>(out_buffer.data()), static_cast<std::streamsize>(file_size));
        return static_cast<size_t>(in.gcount()) == file_size;
    }

} // namespace engine

#endif // ENGINE_CORE_OS_VFS_INL
