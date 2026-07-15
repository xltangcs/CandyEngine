#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static constexpr uint32_t PAK_MAGIC = 0x4B415000; // 'PAK\0'
static constexpr uint32_t PAK_VERSION = 1;

struct Entry
{
    std::string path;
    uint64_t offset = 0;
    uint64_t size = 0;
};

static void CollectFiles(const std::filesystem::path& dir, const std::filesystem::path& base, std::vector<Entry>& entries)
{
    for (auto& e : std::filesystem::recursive_directory_iterator(dir))
    {
        if (!e.is_regular_file())
            continue;

        Entry entry;
        entry.path = std::filesystem::relative(e.path(), base).generic_string();
        entry.size = std::filesystem::file_size(e.path());
        entries.push_back(entry);
    }
}

static int Pack(const std::string& inputDir, const std::string& outputPath)
{
    if (!std::filesystem::is_directory(inputDir))
    {
        std::cerr << "Error: input directory does not exist: " << inputDir << "\n";
        return 1;
    }

    std::vector<Entry> entries;
    CollectFiles(inputDir, inputDir, entries);

    if (entries.empty())
    {
        std::cerr << "Warning: no files found in " << inputDir << "\n";
    }

    // Calculate offsets: header + entry table first, then file data
    uint32_t entryCount = static_cast<uint32_t>(entries.size());
    uint64_t headerSize = sizeof(PAK_MAGIC) + sizeof(PAK_VERSION) + sizeof(entryCount);
    uint64_t entryTableSize = 0;
    for (auto& e : entries)
    {
        entryTableSize += sizeof(uint32_t);           // pathLen
        entryTableSize += e.path.size();              // path
        entryTableSize += sizeof(uint64_t);           // offset
        entryTableSize += sizeof(uint64_t);           // size
    }

    uint64_t dataOffset = headerSize + entryTableSize;
    uint64_t currentOffset = dataOffset;
    for (auto& e : entries)
    {
        e.offset = currentOffset;
        currentOffset += e.size;
    }

    // Write pak file
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open())
    {
        std::cerr << "Error: cannot create output file: " << outputPath << "\n";
        return 1;
    }

    // Header
    out.write(reinterpret_cast<const char*>(&PAK_MAGIC), sizeof(PAK_MAGIC));
    out.write(reinterpret_cast<const char*>(&PAK_VERSION), sizeof(PAK_VERSION));
    out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

    // Entry table
    for (auto& e : entries)
    {
        uint32_t pathLen = static_cast<uint32_t>(e.path.size());
        out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        out.write(e.path.data(), pathLen);
        out.write(reinterpret_cast<const char*>(&e.offset), sizeof(e.offset));
        out.write(reinterpret_cast<const char*>(&e.size), sizeof(e.size));
    }

    // File data
    for (auto& e : entries)
    {
        std::ifstream in(std::filesystem::path(inputDir) / e.path, std::ios::binary);
        if (!in.is_open())
        {
            std::cerr << "Error: cannot read file: " << e.path << "\n";
            return 1;
        }
        out << in.rdbuf();
    }

    out.close();

    std::cout << "Packed " << entryCount << " files into " << outputPath << "\n";
    std::cout << "Total size: " << currentOffset << " bytes\n";
    return 0;
}

static int List(const std::string& pakPath)
{
    std::ifstream file(pakPath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Error: cannot open pak file: " << pakPath << "\n";
        return 1;
    }

    uint32_t magic = 0, version = 0, entryCount = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

    if (magic != PAK_MAGIC)
    {
        std::cerr << "Error: invalid pak file\n";
        return 1;
    }

    std::cout << "Pak file: " << pakPath << "\n";
    std::cout << "Version: " << version << "\n";
    std::cout << "Entries: " << entryCount << "\n\n";

    for (uint32_t i = 0; i < entryCount; i++)
    {
        uint32_t pathLen = 0;
        file.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        std::string path(pathLen, '\0');
        file.read(path.data(), pathLen);
        uint64_t offset = 0, size = 0;
        file.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        file.read(reinterpret_cast<char*>(&size), sizeof(size));

        std::cout << "  " << path << "  (" << size << " bytes)\n";
    }

    return 0;
}

static void PrintUsage()
{
    std::cout << "Usage:\n";
    std::cout << "  PakTool pack <input_dir> <output.pak>   Pack a directory into a .pak file\n";
    std::cout << "  PakTool list <input.pak>                List contents of a .pak file\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "pack" && argc == 4)
    {
        return Pack(argv[2], argv[3]);
    }
    else if (command == "list" && argc == 3)
    {
        return List(argv[2]);
    }
    else
    {
        PrintUsage();
        return 1;
    }
}
