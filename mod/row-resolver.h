#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace native::quick_rows {

struct Layout {
  uint32_t setter;
  uint32_t extent;
  uint32_t countOffset;
  uint32_t vtable;
  uint8_t interfaceOffset;
};

class Image {
  std::span<const uint8_t> bytes_;

public:
  explicit Image(std::span<const uint8_t> bytes) : bytes_(bytes) {}

  std::span<const uint8_t> At(size_t rva, size_t length) const {
    if (rva > bytes_.size() || length > bytes_.size() - rva)
      return {};
    return bytes_.subspan(rva, length);
  }

  template <typename T>
  std::optional<T> Read(size_t rva) const {
    auto data = At(rva, sizeof(T));
    if (data.empty())
      return {};
    T value;
    std::memcpy(&value, data.data(), sizeof(value));
    return value;
  }
};

struct Section {
  uint32_t rva = 0;
  uint32_t size = 0;

  bool Contains(uint64_t start, size_t length) const {
    return size && start >= rva && start - rva <= size && length <= size - (start - rva);
  }
};

// An instruction pattern with "??" for a displacement. The known layouts
// share the opcode sequence, but the executable sections move between builds.
inline bool Matches(std::span<const uint8_t> bytes, std::string_view pattern) {
  auto nibble = [](char digit) {
    return digit >= 'a' ? digit - 'a' + 10 : digit - '0';
  };
  for (auto b : bytes) {
    if (pattern.size() < 2 || (pattern[0] != '?' &&
        b != (nibble(pattern[0]) * 16 + nibble(pattern[1]))))
      return false;
    pattern.remove_prefix(2);
    if (!pattern.empty() && pattern.front() == ' ')
      pattern.remove_prefix(1);
  }
  return pattern.empty();
}

inline constexpr std::string_view setterPattern =
    "40 53 48 83 ec 20 48 8b d9 39 91 ?? ?? ?? ?? 74 5a 89 91 ?? ?? ?? ??"
    " 48 8d 15 ?? ?? ?? ?? 48 8d 4c 24 30 e8 ?? ?? ?? ?? 48 8b d0 48 8b cb e8 ?? ?? ?? ??"
    " 48 8d 15 ?? ?? ?? ?? 48 8d 4c 24 30 e8 ?? ?? ?? ?? 48 8b d0 48 8b cb e8 ?? ?? ?? ??"
    " 48 8d 15 ?? ?? ?? ?? 48 8d 4c 24 30 e8 ?? ?? ?? ?? 48 8b d0 48 8b cb e8 ?? ?? ?? ??"
    " 48 83 c4 20 5b c3";

inline constexpr std::string_view extentPattern =
    "40 53 48 83 ec 40 48 8d 99 d8 00 00 00 0f 29 74 24 30 48 83 3b 00 0f 29 7c 24 20 74 40"
    " 66 0f 6e b1 ?? ?? ?? ?? f3 0f e6 f6 f2 0f 59 35 ?? ?? ?? ?? e8 ?? ?? ?? ??"
    " 48 8b cb 66 0f 6e f8 f3 0f e6 ff f2 0f 59 fe e8 ?? ?? ?? ??"
    " 66 0f 2e f8 7a 02 74 0b 0f 28 cf 48 8b cb e8 ?? ?? ?? ??"
    " 0f 28 74 24 30 0f 28 7c 24 20 48 83 c4 40 5b c3";

// Some layouts call a page-height helper instead of multiplying the row count inline.
inline constexpr std::string_view extentUsingPageHeightPattern =
    "48 89 5c 24 08 57 48 83 ec 40 48 8d 99 ?? ?? ?? ?? 0f 29 74 24 30 48 83 3b 00"
    " 48 8b f9 0f 29 7c 24 20 74 37 e8 ?? ?? ?? ?? 48 8b cf 66 0f 6e f0 f3 0f e6 f6"
    " e8 ?? ?? ?? ?? 48 8b cb 0f 28 f8 f2 0f 59 fe e8 ?? ?? ?? ??"
    " 66 0f 2e f8 7a 02 74 0b 0f 28 cf 48 8b cb e8 ?? ?? ?? ??"
    " 48 8b 5c 24 50 0f 28 74 24 30 0f 28 7c 24 20 48 83 c4 40 5f c3";

inline constexpr std::string_view pageHeightPattern =
    "48 8b c4 48 89 58 18 48 89 70 20 57 48 83 ec 20 48 8b d9 33 ff 89 78 08"
    " 40 38 b9 ?? ?? ?? ?? 74 34 48 81 c1 ?? ?? ?? ?? 48 39 39 74 28 48 8d 50 10"
    " e8 ?? ?? ?? ?? 90 bf 01 00 00 00 89 7c 24 30 48 8b c8 e8 ?? ?? ?? ??"
    " 3b 83 ?? ?? ?? ?? 77 05 40 8a f7 eb 03 40 32 f6 40 f6 c7 01 74 0a 48 8d 4c 24 38"
    " e8 ?? ?? ?? ?? 40 84 f6 74 0a f2 0f 10 83 ?? ?? ?? ?? eb 14"
    " 66 0f 6e 83 ?? ?? ?? ?? f3 0f e6 c0 f2 0f 59 83 ?? ?? ?? ??"
    " 48 8b 5c 24 40 48 8b 74 24 48 48 83 c4 20 5f c3";

inline constexpr size_t PatternLength(std::string_view pattern) {
  return (pattern.size() + 1) / 3;
}

inline int64_t RipTarget(const Image& image, uint32_t displacementAt) {
  auto displacement = image.Read<int32_t>(displacementAt);
  return displacement ? int64_t(displacementAt) + 4 + *displacement : -1;
}

inline std::optional<uint32_t> ExtentRowCountOffset(const Image& image, Section text,
                                                     uint32_t start, size_t length) {
  auto body = image.At(start, length);
  if (length == PatternLength(extentPattern) && Matches(body, extentPattern))
    return image.Read<uint32_t>(start + 33);
  if (length != PatternLength(extentUsingPageHeightPattern) ||
      !Matches(body, extentUsingPageHeightPattern))
    return {};

  auto helper = RipTarget(image, start + 53);
  if (helper < 0 || !text.Contains(helper, PatternLength(pageHeightPattern)) ||
      !Matches(image.At(helper, PatternLength(pageHeightPattern)), pageHeightPattern))
    return {};
  auto count = *image.Read<uint32_t>(helper + 123);
  // Check the column count, tile-height and repeater fields against the row field.
  if (count < 0x10 || count > 1024 || image.Read<uint32_t>(helper + 74) != count - 0x10 ||
      image.Read<uint32_t>(helper + 113) != count - 8 ||
      image.Read<uint32_t>(helper + 135) != count - 8 ||
      image.Read<uint32_t>(start + 13) != count + 0x30)
    return {};
  return count;
}

inline bool RefersTo(const Image& image, Section readOnly, uint32_t displacementAt,
                     std::wstring_view text) {
  auto target = RipTarget(image, displacementAt);
  size_t length = (text.size() + 1) * sizeof(wchar_t);
  if (target < 0 || !readOnly.Contains(target, length))
    return false;
  auto bytes = image.At(target, length);
  return bytes.size() == length && bytes[length - 1] == 0 && bytes[length - 2] == 0 &&
         std::memcmp(bytes.data(), text.data(), text.size() * sizeof(wchar_t)) == 0;
}

inline std::optional<std::pair<uint32_t, uint8_t>> FindGridTable(
    const Image& image, uintptr_t loadAddress, Section text, Section readOnly,
    IMAGE_DATA_DIRECTORY exceptions, uint32_t countOffset) {
  // Find the constructor that stores the implementation vtable and initializes
  // the same row count field. The class name distinguishes this table from
  // those of nearby WinRT interfaces. The thunk exposes the interface offset.
  constexpr auto storeVtable = std::string_view("48 8d 05 ?? ?? ?? ?? 48 89 03");
  constexpr auto initRows = std::string_view("48 c7 83 ?? ?? ?? ?? 02 00 00 00");
  constexpr auto initColumnsHeightAndRows = std::string_view(
      "c7 83 ?? ?? ?? ?? 03 00 00 00 48 b8 ?? ?? ?? ?? ?? ?? ?? ??"
      " 48 89 83 ?? ?? ?? ?? c7 83 ?? ?? ?? ?? 02 00 00 00");
  constexpr auto interfaceThunk = std::string_view("48 8d 51 ?? 48 f7 d9 48 1b c0 48 23 c2 c3");
  constexpr auto classNameEntry = std::string_view("40 53 48 83 ec 30 48 8b da 48 8d 15");
  auto textEntry = [&](size_t slot, size_t length) -> std::optional<uint32_t> {
    auto address = image.Read<uintptr_t>(slot);
    if (!address || *address < loadAddress || !text.Contains(*address - loadAddress, length))
      return {};
    return uint32_t(*address - loadAddress);
  };

  std::optional<std::pair<uint32_t, uint8_t>> result;
  auto end = uint64_t(exceptions.VirtualAddress) + exceptions.Size;
  for (size_t i = exceptions.VirtualAddress; i < end; i += sizeof(RUNTIME_FUNCTION)) {
    auto entry = image.Read<RUNTIME_FUNCTION>(i);
    if (!entry || entry->BeginAddress >= entry->EndAddress || entry->EndAddress - entry->BeginAddress < 21 ||
        !text.Contains(entry->BeginAddress, entry->EndAddress - entry->BeginAddress))
      continue;
    for (auto ip = entry->BeginAddress; ip <= entry->EndAddress - 21; ++ip) {
      auto first = image.Read<uint8_t>(ip);
      if (first != 0x48 || !Matches(image.At(ip, PatternLength(storeVtable)), storeVtable))
        continue;
      auto init = ip + PatternLength(storeVtable);
      bool rowsInitialized = Matches(image.At(init, PatternLength(initRows)), initRows) &&
                             image.Read<uint32_t>(init + 3) == countOffset;
      if (!rowsInitialized && countOffset >= 0x10 &&
          entry->EndAddress - init >= PatternLength(initColumnsHeightAndRows)) {
        rowsInitialized = Matches(image.At(init, PatternLength(initColumnsHeightAndRows)),
                                  initColumnsHeightAndRows) &&
                          image.Read<uint32_t>(init + 2) == countOffset - 0x10 &&
                          image.Read<uint32_t>(init + 23) == countOffset - 8 &&
                          image.Read<uint32_t>(init + 29) == countOffset;
      }
      if (!rowsInitialized)
        continue;
      auto table = RipTarget(image, ip + 3);
      if (table < 0 || !readOnly.Contains(table, 6 * sizeof(void*)) || table % alignof(void*))
        continue;
      auto slot0 = textEntry(table, 6);
      auto adjuster = textEntry(table + 3 * sizeof(void*), PatternLength(interfaceThunk));
      auto runtimeName = textEntry(table + 5 * sizeof(void*), PatternLength(classNameEntry) + 4);
      if (!slot0 || !adjuster || !runtimeName ||
          !Matches(image.At(*slot0, 6), "b8 02 40 00 80 c3") ||
          !Matches(image.At(*adjuster, PatternLength(interfaceThunk)), interfaceThunk) ||
          !Matches(image.At(*runtimeName, PatternLength(classNameEntry)), classNameEntry) ||
          !RefersTo(image, readOnly, *runtimeName + 12, L"ControlCenter.PaginatedGridView"))
        continue;
      auto delta = *image.Read<uint8_t>(*adjuster + 3);
      if (!delta || delta > 0x40 || delta % alignof(void*))
        continue;
      auto candidate = std::pair(uint32_t(table), delta);
      if (result && *result != candidate)
        return {};
      result = candidate;
    }
  }
  return result;
}

inline std::optional<Layout> Resolve(std::span<const uint8_t> mappedImage, uintptr_t loadAddress) {
  Image image(mappedImage);
  auto dos = image.Read<IMAGE_DOS_HEADER>(0);
  if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
    return {};
  size_t peOffset = dos->e_lfanew;
  auto pe = image.Read<IMAGE_NT_HEADERS64>(peOffset);
  if (!pe || pe->Signature != IMAGE_NT_SIGNATURE ||
      pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
      pe->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER64) ||
      pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      pe->OptionalHeader.SizeOfImage != mappedImage.size() ||
      pe->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXCEPTION)
    return {};

  Section text, readOnly, unwind;
  auto sectionOffset = peOffset + sizeof(IMAGE_NT_HEADERS64);
  for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; ++i) {
    auto header = image.Read<IMAGE_SECTION_HEADER>(sectionOffset + i * sizeof(IMAGE_SECTION_HEADER));
    if (!header)
      return {};
    Section section{header->VirtualAddress, header->Misc.VirtualSize};
    if (image.At(section.rva, section.size).size() != section.size)
      return {};
    if (std::memcmp(header->Name, ".text", sizeof(".text")) == 0 &&
        (header->Characteristics & IMAGE_SCN_MEM_EXECUTE) &&
        (header->Characteristics & IMAGE_SCN_MEM_READ))
      text = section;
    if (std::memcmp(header->Name, ".rdata", sizeof(".rdata")) == 0 &&
        (header->Characteristics & IMAGE_SCN_MEM_READ) &&
        !(header->Characteristics & IMAGE_SCN_MEM_WRITE))
      readOnly = section;
    if (std::memcmp(header->Name, ".pdata", sizeof(".pdata")) == 0 &&
        (header->Characteristics & IMAGE_SCN_MEM_READ))
      unwind = section;
  }
  auto exceptions = pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
  if (!text.size || !readOnly.size || !exceptions.Size ||
      exceptions.Size % sizeof(RUNTIME_FUNCTION) ||
      !unwind.Contains(exceptions.VirtualAddress, exceptions.Size))
    return {};

  Layout result{};
  uint32_t extentCountOffset = 0;
  auto end = uint64_t(exceptions.VirtualAddress) + exceptions.Size;
  for (size_t i = exceptions.VirtualAddress; i < end; i += sizeof(RUNTIME_FUNCTION)) {
    auto entry = image.Read<RUNTIME_FUNCTION>(i);
    if (!entry || entry->BeginAddress >= entry->EndAddress)
      continue;
    uint32_t start = entry->BeginAddress;
    size_t length = entry->EndAddress - start;
    if (!text.Contains(start, length))
      continue;
    if (length == PatternLength(setterPattern) && Matches(image.At(start, length), setterPattern)) {
      auto offset = *image.Read<uint32_t>(start + 11);
      if (offset < sizeof(void*) || offset > 1024 || offset % alignof(int32_t) ||
          image.Read<uint32_t>(start + 19) != offset ||
          !RefersTo(image, readOnly, start + 26, L"PageHeight") ||
          !RefersTo(image, readOnly, start + 54, L"NumberOfPages") ||
          !RefersTo(image, readOnly, start + 82, L"QuickActionsPagerVisibility"))
        continue;
      if (result.setter)
        return {};
      result.setter = start;
      result.countOffset = offset;
    }
    if (auto offset = ExtentRowCountOffset(image, text, start, length)) {
      if (result.extent)
        return {};
      result.extent = start;
      extentCountOffset = *offset;
    }
  }
  if (!result.setter || !result.extent || result.countOffset != extentCountOffset)
    return {};
  auto table = FindGridTable(image, loadAddress, text, readOnly, exceptions, result.countOffset);
  if (!table)
    return {};
  result.vtable = table->first;
  result.interfaceOffset = table->second;
  return result;
}

}  // namespace native::quick_rows
