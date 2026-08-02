#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

static constexpr uint32_t PAK_MAGIC = 0x4B415000; // 'PAK\0'
static constexpr uint32_t PAK_VERSION = 1;

struct Entry
{
    std::string path;
    std::string sourcePath;
    uint64_t offset = 0;
    uint64_t size = 0;
};

static void CollectDir(const std::filesystem::path& dir, const std::string& subdir, std::vector<Entry>& entries)
{
    if (!std::filesystem::is_directory(dir))
    {
        std::cerr << "Error: input directory does not exist: " << dir.string() << "\n";
        return;
    }
    size_t before = entries.size();
    for (auto& e : std::filesystem::recursive_directory_iterator(dir))
    {
        if (!e.is_regular_file())
            continue;
        std::string rel = std::filesystem::relative(e.path(), dir).generic_string();
        Entry entry;
        entry.path = subdir.empty() ? rel : (subdir + "/" + rel);
        entry.sourcePath = e.path().string();
        entry.size = std::filesystem::file_size(e.path());
        entries.push_back(entry);
    }
    std::cout << "Collected " << (entries.size() - before) << " files from '" << dir.string()
              << "' under '" << subdir << "/'\n";
}

static int Pack(int argc, char* argv[])
{
    std::vector<Entry> entries;
    std::vector<std::pair<std::string, std::string>> dirs; // (subdir, dir)
    std::string outputPath;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--subdir")
        {
            if (i + 2 >= argc)
            {
                std::cerr << "Error: --subdir requires <name> <dir>\n";
                return 1;
            }
            std::string subdir = argv[i + 1];
            std::string dir = argv[i + 2];
            i += 2;
            dirs.emplace_back(subdir, dir);
        }
        else
        {
            outputPath = arg;
        }
    }

    if (dirs.empty())
    {
        std::cerr << "Error: no input directories provided (use --subdir <name> <dir>).\n";
        return 1;
    }
    if (outputPath.empty())
    {
        std::cerr << "Error: no output .pak path provided.\n";
        return 1;
    }

    for (auto& [subdir, dir] : dirs)
        CollectDir(dir, subdir, entries);

    if (entries.empty())
    {
        std::cerr << "Error: no files collected, nothing to pack.\n";
        return 1;
    }

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.path < b.path; });
    entries.erase(std::unique(entries.begin(), entries.end(),
                              [](const Entry& a, const Entry& b) { return a.path == b.path; }),
                  entries.end());

    uint32_t entryCount = static_cast<uint32_t>(entries.size());
    uint64_t headerSize = sizeof(PAK_MAGIC) + sizeof(PAK_VERSION) + sizeof(entryCount);
    uint64_t entryTableSize = 0;
    for (auto& e : entries)
    {
        entryTableSize += sizeof(uint32_t);
        entryTableSize += e.path.size();
        entryTableSize += sizeof(uint64_t);
        entryTableSize += sizeof(uint64_t);
    }

    uint64_t dataOffset = headerSize + entryTableSize;
    uint64_t currentOffset = dataOffset;
    for (auto& e : entries)
    {
        e.offset = currentOffset;
        currentOffset += e.size;
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open())
    {
        std::cerr << "Error: cannot create output file: " << outputPath << "\n";
        return 1;
    }

    out.write(reinterpret_cast<const char*>(&PAK_MAGIC), sizeof(PAK_MAGIC));
    out.write(reinterpret_cast<const char*>(&PAK_VERSION), sizeof(PAK_VERSION));
    out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

    for (auto& e : entries)
    {
        uint32_t pathLen = static_cast<uint32_t>(e.path.size());
        out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        out.write(e.path.data(), pathLen);
        out.write(reinterpret_cast<const char*>(&e.offset), sizeof(e.offset));
        out.write(reinterpret_cast<const char*>(&e.size), sizeof(e.size));
    }

    for (auto& e : entries)
    {
        std::ifstream in(e.sourcePath, std::ios::binary);
        if (!in.is_open())
        {
            std::cerr << "Error: cannot read source file: " << e.sourcePath << "\n";
            out.close();
            std::filesystem::remove(outputPath);
            return 1;
        }
        out << in.rdbuf();
        in.close();
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
    std::cout << "  PakTool pack --subdir <name> <input_dir> [--subdir <name> <input_dir> ...] <output.pak>\n";
    std::cout << "    Pack one or more directories into a .pak file.\n";
    std::cout << "    Each directory's files are stored under '<name>/'.\n";
    std::cout << "    e.g. PakTool pack --subdir engine Candy/Content --subdir game JumpGame/Content JumpGame.pak\n";
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

    if (command == "pack")
    {
        return Pack(argc, argv);
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
