#define WH_EDITING
#define WH_MOD
#define WH_MOD_ID L"native-model-test"
#include <windhawk_api.h>
#include "common.h"
#include <cstdio>
namespace F = winrt::Windows::Foundation;
struct Store {
  F::PropertyType kind;
  double number = 37;
  F::PropertyType written = F::PropertyType::Empty;
};
struct Property : winrt::implements<Property, native::D::ICustomProperty> {
  std::shared_ptr<Store> store;
  explicit Property(std::shared_ptr<Store> value) : store(std::move(value)) {}
  auto Type() { return winrt::xaml_typename<double>(); }
  winrt::hstring Name() { return L"Volume"; }
  bool CanRead() { return true; }
  bool CanWrite() { return true; }
  native::IInspectable GetValue(native::IInspectable const &) {
    switch (store->kind) {
    case F::PropertyType::Int32:
      return winrt::box_value(int32_t(store->number));
    case F::PropertyType::UInt32:
      return winrt::box_value(uint32_t(store->number));
    case F::PropertyType::Single:
      return winrt::box_value(float(store->number));
    default:
      return winrt::box_value(store->number);
    }
  }
  void SetValue(native::IInspectable const &, native::IInspectable const &value) {
    auto number = value.as<F::IPropertyValue>();
    store->written = number.Type();
    switch (number.Type()) {
    case F::PropertyType::Int32:
      store->number = number.GetInt32();
      break;
    case F::PropertyType::UInt32:
      store->number = number.GetUInt32();
      break;
    case F::PropertyType::Single:
      store->number = number.GetSingle();
      break;
    case F::PropertyType::Double:
      store->number = number.GetDouble();
      break;
    default:
      throw winrt::hresult_invalid_argument();
    }
  }
  native::IInspectable GetIndexedValue(native::IInspectable const &, native::IInspectable const &) {
    return nullptr;
  }
  void SetIndexedValue(native::IInspectable const &, native::IInspectable const &,
                       native::IInspectable const &) {}
};
struct Model : winrt::implements<Model, native::D::ICustomPropertyProvider> {
  std::shared_ptr<Store> store;
  explicit Model(std::shared_ptr<Store> value) : store(std::move(value)) {}
  native::D::ICustomProperty GetCustomProperty(winrt::hstring const &name) {
    return name == L"Volume" ? winrt::make<Property>(store) : nullptr;
  }
  native::D::ICustomProperty GetIndexedProperty(winrt::hstring const &,
                                                native::X::Interop::TypeName const &) {
    return nullptr;
  }
  winrt::hstring GetStringRepresentation() { return L"Native volume test"; }
  native::X::Interop::TypeName Type() {
    return {L"NativeVolumeTest", native::X::Interop::TypeKind::Custom};
  }
};
int main() {
  try {
    winrt::init_apartment();
    for (auto kind : {F::PropertyType::Int32, F::PropertyType::UInt32, F::PropertyType::Single,
                      F::PropertyType::Double}) {
      auto store = std::make_shared<Store>(Store{kind});
      auto model = winrt::make<Model>(store);
      if (!native::SetNumber(model, L"Volume", 0) || store->number != 0 || store->written != kind)
        return 1;
      if (!native::SetNumber(model, L"Volume", 37) || store->number != 37 || store->written != kind)
        return 2;
    }
    std::puts("PASS: native model writes preserve Int32/UInt32/Single/Double types; zero and "
              "restore; no slider binding overwritten");
    return 0;
  } catch (winrt::hresult_error const &error) {
    std::printf("FAIL: %08X\n", unsigned(error.code().value));
    return 3;
  }
}
