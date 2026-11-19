#pragma once
#include "row-resolver.h"
#include <memory>
#include <winrt/Windows.Foundation.h>

namespace native::quick_rows {

inline bool Readable(uintptr_t address, size_t length) {
  MEMORY_BASIC_INFORMATION region{};
  if (!VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) ||
      region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD))
    return false;
  auto protection = region.Protect & 0xFF;
  if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
      protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
      protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
    return false;
  auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);
  return address >= base && address - base <= region.RegionSize &&
         length <= region.RegionSize - (address - base);
}

struct GridBinding {
  winrt::com_ptr<::IUnknown> pin;
  void* instance;
  int rows;
};

inline std::optional<GridBinding> CheckInterface(winrt::com_ptr<::IUnknown> candidate,
                                                 uintptr_t loadAddress, Layout const &layout) {
  auto address = reinterpret_cast<uintptr_t>(candidate.get());
  if (address < layout.interfaceOffset)
    return {};
  auto base = address - layout.interfaceOffset;
  if (base % alignof(void*) || !Readable(base, layout.countOffset + sizeof(int32_t)))
    return {};
  uintptr_t vtable;
  std::memcpy(&vtable, reinterpret_cast<const void*>(base), sizeof(vtable));
  if (vtable != loadAddress + layout.vtable)
    return {};
  int32_t rows;
  std::memcpy(&rows, reinterpret_cast<const void*>(base + layout.countOffset), sizeof(rows));
  if (rows != 1 && rows != 2)
    return {};
  return GridBinding{std::move(candidate), reinterpret_cast<void*>(base), rows};
}

inline std::optional<GridBinding> BindGrid(winrt::Windows::Foundation::IInspectable const &grid,
                                           uintptr_t loadAddress, Layout const &layout) {
  if (!grid || winrt::get_class_name(grid) != L"ControlCenter.PaginatedGridView")
    return {};
  auto root = reinterpret_cast<winrt::impl::inspectable_abi*>(winrt::get_abi(grid));
  // A successful QueryInterface pins the real object while an override is active.
  constexpr winrt::guid knownInterface{0xad3ad37e, 0x5360, 0x5f2c, {0x86, 0x50, 0x6d, 0xd9, 0x61, 0x6e, 0x1d, 0x71}};
  auto query = [&](winrt::guid const& iid) -> std::optional<GridBinding> {
    winrt::com_ptr<::IUnknown> candidate;
    if (FAILED(root->QueryInterface(iid, candidate.put_void())))
      return {};
    return CheckInterface(std::move(candidate), loadAddress, layout);
  };
  if (auto known = query(knownInterface))
    return known;

  uint32_t count = 0;
  winrt::guid* raw = nullptr;
  if (FAILED(root->GetIids(&count, &raw)))
    return {};
  struct FreeIids {
    void operator()(winrt::guid* p) const { CoTaskMemFree(p); }
  };
  std::unique_ptr<winrt::guid, FreeIids> iids(raw);
  if (!iids || count > 128)
    return {};
  for (auto const& iid : std::span(iids.get(), count))
    if (auto match = query(iid))
      return match;
  return {};
}

}  // namespace native::quick_rows
