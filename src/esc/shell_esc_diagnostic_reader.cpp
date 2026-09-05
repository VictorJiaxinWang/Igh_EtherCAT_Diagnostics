#include "ethercat_diag/esc/esc_diagnostic_reader.h"

#include <utility>

// Compatibility adapter for migration tests only. Production targets do not
// link this translation unit or the shell-based EscRegisterReader backend.
EscDiagnosticReader::EscDiagnosticReader(
    EscRegisterReader register_reader)
    : read_register_(
          [reader = std::move(register_reader)](
              int master_index,
              int slave_position,
              std::uint16_t address) {
              return reader.readU16(
                  master_index,
                  slave_position,
                  address);
          })
{
}
