#include <cstdint>
#include <generator>
#include <inplace_vector>
#include <memory>
#include <ranges>
#include <vector>

namespace Manadrain {
struct FunctionType {
  std::vector<std::uint8_t> arguments;
  std::optional<std::uint8_t> return_type;
};

struct FunctionEntry {
  const FunctionType *signature;
};

struct FunctionImport {
  std::string module_name;
  std::string field_name;
  const FunctionEntry *entry;
};

struct FunctionTable {
  std::uint32_t minimum;
  std::uint32_t maximum;
};

struct MemoryEntry {
  std::uint32_t minimum;
  std::uint32_t maximum;
};

struct GlobalEntry {
  std::uint8_t data_type;
  bool allow_mut;
  std::vector<std::uint8_t> init;
};

struct FunctionExport {
  std::string export_name;
  const FunctionEntry *entry;
};

struct MemoryExport {
  std::string export_name;
  const MemoryEntry *entry;
};

struct TableExport {
  std::string export_name;
  const FunctionTable *entry;
};

class Parser {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> binary_buffer;
  void parse_binary();

private:
  std::size_t position;

  std::generator<std::uint8_t> traverse();
  std::uint8_t forward();

  std::uint32_t take_vars32();
  std::uint32_t take_varu32();

  std::vector<std::unique_ptr<const FunctionType>> function_types;
  std::vector<std::unique_ptr<const FunctionImport>> function_imports;
  std::vector<std::unique_ptr<const FunctionTable>> function_tables;
  std::vector<std::unique_ptr<const FunctionExport>> function_exports;
  std::vector<std::unique_ptr<const FunctionEntry>> function_entries;

  std::vector<std::unique_ptr<const MemoryEntry>> memory_entries;
  std::vector<std::unique_ptr<const MemoryExport>> memory_exports;

  std::vector<std::unique_ptr<const TableExport>> table_exports;

  std::vector<std::unique_ptr<const GlobalEntry>> global_entries;

  void parse_function_type();
  void parse_import_entry();
  void parse_table_entry();
  void parse_function_entry();
  void parse_memory_entry();
  void parse_global_entry();
  void parse_export_entry();
};
} // namespace Manadrain
