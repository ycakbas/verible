// Copyright 2017-2026 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Test cases in this file should be *insensitive* to wrapping penalties.
// Penalty-sensitive tests belong in formatter-tuning_test.cc.
// Thematic end-to-end cases live in sibling formatter_*_test.cc files.
// New GitHub-issue regressions belong in formatter_issue_regression_test.cc.

#include "verible/verilog/formatting/formatter.h"

#include <memory>
#include <sstream>
#include <string_view>

#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter-test-utils.h"

namespace verilog {
namespace formatter {

// private, extern function in formatter.cc, directly tested here.
extern absl::Status VerifyFormatting(
    const verible::TextStructureView &text_structure,
    std::string_view formatted_output, std::string_view filename);

namespace {

static constexpr VerilogPreprocess::Config kDefaultPreprocess;

using absl::StatusCode;

TEST(VerifyFormattingTest, NoError) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const auto status = VerifyFormatting(text_structure, code, "<filename>");
  EXPECT_OK(status);
}

// Tests that un-lexable outputs are caught as errors.
TEST(VerifyFormattingTest, LexError) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const std::string_view bad_code("1class c;endclass\n");  // lexical error
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

// Tests that un-parseable outputs are caught as errors.
TEST(VerifyFormattingTest, ParseError) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const std::string_view bad_code("classc;endclass\n");  // syntax error
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

// Tests that lexical differences are caught as errors.
TEST(VerifyFormattingTest, LexicalDifference) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const std::string_view bad_code("class c;;endclass\n");  // different tokens
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

TEST(FormatterTest, FormatCustomStyleTest) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"module m;wire w;endmodule\n",
       "module m;\n"
       "          wire w;\n"
       "endmodule\n"},
  };

  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 10;  // unconventional indentation
  style.wrap_spaces = 4;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status);
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Small smoke subset; thematic cases live in sibling formatter_*_test.cc files.
static constexpr FormatterTestCase kSmokeFormatterTestCases[] = {
    {"", ""},
    {"\n", "\n"},
    {"\n\n", "\n\n"},
    {"\t//comment\n", "//comment\n"},
    {"\t/*comment*/\n", "/*comment*/\n"},
    {"\t/*multi-line\ncomment*/\n", "/*multi-line\ncomment*/\n"},
};

TEST(FormatterEndToEndTest, SmokeFormatterTestCases) {
  RunFormatterTestCases40(kSmokeFormatterTestCases);
}

TEST(FormatterEndToEndTest, NamedPortMinimumSpacingTest) {
  const char input[] =
      "module m;\n"
      "backend u_backend (\n"
      ".clk(clk),\n"
      ".rst_n(rst_n),\n"
      ".control_flow_blocked(control_flow_blocked),\n"
      ".machine_software_interrupt(machine_software_interrupt),\n"
      ".machine_timer_interrupt(machine_timer_interrupt)\n"
      ");\n"
      "endmodule\n";

  {
    // Default spacing 0:
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.named_port_alignment = AlignmentPolicy::kAlign;
    style.named_port_indentation = IndentationStyle::kIndent;
    style.named_port_minimum_spacing = 0;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module m;\n"
              "  backend u_backend (\n"
              "    .clk                       (clk),\n"
              "    .rst_n                     (rst_n),\n"
              "    .control_flow_blocked      (control_flow_blocked),\n"
              "    .machine_software_interrupt(machine_software_interrupt),\n"
              "    .machine_timer_interrupt   (machine_timer_interrupt)\n"
              "  );\n"
              "endmodule\n");
  }
  {
    // Spacing 1:
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.named_port_alignment = AlignmentPolicy::kAlign;
    style.named_port_indentation = IndentationStyle::kIndent;
    style.named_port_minimum_spacing = 1;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module m;\n"
              "  backend u_backend (\n"
              "    .clk                        (clk),\n"
              "    .rst_n                      (rst_n),\n"
              "    .control_flow_blocked       (control_flow_blocked),\n"
              "    .machine_software_interrupt (machine_software_interrupt),\n"
              "    .machine_timer_interrupt    (machine_timer_interrupt)\n"
              "  );\n"
              "endmodule\n");
  }
  {
    // Spacing 2:
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.named_port_alignment = AlignmentPolicy::kAlign;
    style.named_port_indentation = IndentationStyle::kIndent;
    style.named_port_minimum_spacing = 2;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module m;\n"
              "  backend u_backend (\n"
              "    .clk                         (clk),\n"
              "    .rst_n                       (rst_n),\n"
              "    .control_flow_blocked        (control_flow_blocked),\n"
              "    .machine_software_interrupt  (machine_software_interrupt),\n"
              "    .machine_timer_interrupt     (machine_timer_interrupt)\n"
              "  );\n"
              "endmodule\n");
  }
}

TEST(FormatterEndToEndTest, NamedParameterMinimumSpacingTest) {
  const char input[] =
      "module m;\n"
      "foo #(\n"
      ".WIDTH(32),\n"
      ".SOME_LONG_PARAMETER(VALUE)\n"
      ") u_foo ();\n"
      "endmodule\n";

  {
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.named_parameter_alignment = AlignmentPolicy::kAlign;
    style.named_parameter_indentation = IndentationStyle::kIndent;
    style.named_parameter_minimum_spacing = 0;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module m;\n"
              "  foo #(\n"
              "    .WIDTH              (32),\n"
              "    .SOME_LONG_PARAMETER(VALUE)\n"
              "  ) u_foo ();\n"
              "endmodule\n");
  }
  {
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.named_parameter_alignment = AlignmentPolicy::kAlign;
    style.named_parameter_indentation = IndentationStyle::kIndent;
    style.named_parameter_minimum_spacing = 1;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module m;\n"
              "  foo #(\n"
              "    .WIDTH               (32),\n"
              "    .SOME_LONG_PARAMETER (VALUE)\n"
              "  ) u_foo ();\n"
              "endmodule\n");
  }
}

TEST(FormatterEndToEndTest, PortDeclarationsGroupBoundaryTest) {
  const char input[] =
      "module core (\n"
      "input logic clk,\n"
      "input logic rst_n,\n"
      "\n"
      "// Instruction-memory interface\n"
      "output logic imem_request_valid,\n"
      "input logic imem_request_ready,\n"
      "output bus_pkg::bus_request_t imem_request,\n"
      "\n"
      "// Data-memory request\n"
      "output logic dmem_request_valid,\n"
      "input logic dmem_request_ready,\n"
      "output bus_pkg::bus_request_t dmem_request,\n"
      "\n"
      "// Retirement\n"
      "output logic retire_valid,\n"
      "input logic retire_ready,\n"
      "output commit_pkg::rob_retire_t retire_entry,\n"
      "output commit_pkg::architectural_retire_t architectural_retire_entry\n"
      ");\n"
      "endmodule\n";

  {
    // Default: blank-lines breaks groups
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.port_declarations_alignment = AlignmentPolicy::kAlign;
    style.port_declarations_indentation = IndentationStyle::kIndent;
    style.port_declarations_group_boundary =
        AlignmentGroupBoundary::kBlankLines;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module core (\n"
              "  input logic clk,\n"
              "  input logic rst_n,\n"
              "\n"
              "  // Instruction-memory interface\n"
              "  output logic                  imem_request_valid,\n"
              "  input  logic                  imem_request_ready,\n"
              "  output bus_pkg::bus_request_t imem_request,\n"
              "\n"
              "  // Data-memory request\n"
              "  output logic                  dmem_request_valid,\n"
              "  input  logic                  dmem_request_ready,\n"
              "  output bus_pkg::bus_request_t dmem_request,\n"
              "\n"
              "  // Retirement\n"
              "  output logic                              retire_valid,\n"
              "  input  logic                              retire_ready,\n"
              "  output commit_pkg::rob_retire_t           retire_entry,\n"
              "  output commit_pkg::architectural_retire_t "
              "architectural_retire_entry\n"
              ");\n"
              "endmodule\n");
  }
  {
    // none: all ports in one alignment group
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.port_declarations_alignment = AlignmentPolicy::kAlign;
    style.port_declarations_indentation = IndentationStyle::kIndent;
    style.port_declarations_group_boundary = AlignmentGroupBoundary::kNone;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(
        stream.str(),
        "module core (\n"
        "  input  logic                              clk,\n"
        "  input  logic                              rst_n,\n"
        "\n"
        "  // Instruction-memory interface\n"
        "  output logic                              imem_request_valid,\n"
        "  input  logic                              imem_request_ready,\n"
        "  output bus_pkg::bus_request_t             imem_request,\n"
        "\n"
        "  // Data-memory request\n"
        "  output logic                              dmem_request_valid,\n"
        "  input  logic                              dmem_request_ready,\n"
        "  output bus_pkg::bus_request_t             dmem_request,\n"
        "\n"
        "  // Retirement\n"
        "  output logic                              retire_valid,\n"
        "  input  logic                              retire_ready,\n"
        "  output commit_pkg::rob_retire_t           retire_entry,\n"
        "  output commit_pkg::architectural_retire_t "
        "architectural_retire_entry\n"
        ");\n"
        "endmodule\n");
  }
}

TEST(FormatterEndToEndTest, PortDeclarationsDimensionsTest) {
  const char input[] =
      "module foo (\n"
      "input logic [SOURCE_COUNT-1:0] source_valid,\n"
      "output logic [SOURCE_COUNT-1:0] source_ready,\n"
      "input commit_pkg::rob_completion_t source_completion [SOURCE_COUNT]\n"
      ");\n"
      "endmodule\n";

  {
    // Default: separate
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.port_declarations_alignment = AlignmentPolicy::kAlign;
    style.port_declarations_indentation = IndentationStyle::kIndent;
    style.port_declarations_packed_dimensions =
        PackedDimensionsPlacement::kSeparate;
    style.port_declarations_unpacked_dimensions =
        UnpackedDimensionsPlacement::kSeparate;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module foo (\n"
              "  input  logic                        [SOURCE_COUNT-1:0] "
              "source_valid,\n"
              "  output logic                        [SOURCE_COUNT-1:0] "
              "source_ready,\n"
              "  input  commit_pkg::rob_completion_t                    "
              "source_completion[SOURCE_COUNT]\n"
              ");\n"
              "endmodule\n");
  }
  {
    // attach-to-type packed, attach-to-name unpacked
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.port_declarations_alignment = AlignmentPolicy::kAlign;
    style.port_declarations_indentation = IndentationStyle::kIndent;
    style.port_declarations_packed_dimensions =
        PackedDimensionsPlacement::kAttachToType;
    style.port_declarations_unpacked_dimensions =
        UnpackedDimensionsPlacement::kAttachToName;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module foo (\n"
              "  input  logic [SOURCE_COUNT-1:0]     source_valid,\n"
              "  output logic [SOURCE_COUNT-1:0]     source_ready,\n"
              "  input  commit_pkg::rob_completion_t source_completion "
              "[SOURCE_COUNT]\n"
              ");\n"
              "endmodule\n");
  }
}

TEST(FormatterEndToEndTest, PortDeclarationsMultipleUnpackedDimensionsTest) {
  const char input[] =
      "module m (\n"
      "input foo_t foo [COUNT],\n"
      "input longer_type_t long_name [A][B]\n"
      ");\n"
      "endmodule\n";

  FormatStyle style;
  style.column_limit = 120;
  style.indentation_spaces = 2;
  style.port_declarations_alignment = AlignmentPolicy::kAlign;
  style.port_declarations_indentation = IndentationStyle::kIndent;
  style.port_declarations_packed_dimensions =
      PackedDimensionsPlacement::kAttachToType;
  style.port_declarations_unpacked_dimensions =
      UnpackedDimensionsPlacement::kAttachToName;

  std::ostringstream stream;
  EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
  const std::string formatted = stream.str();
  EXPECT_EQ(formatted,
            "module m (\n"
            "  input foo_t         foo [COUNT],\n"
            "  input longer_type_t long_name [A][B]\n"
            ");\n"
            "endmodule\n");

  // Verify idempotence
  std::ostringstream stream2;
  EXPECT_OK(FormatVerilog(formatted, "<filename>", style, stream2));
  EXPECT_EQ(stream2.str(), formatted);
}

TEST(FormatterEndToEndTest, ModuleNetVariableDimensionsTest) {
  const char input_packed[] =
      "module m;\n"
      "logic [31:0] data;\n"
      "some_really_long_type_t thing;\n"
      "endmodule\n";

  {
    // Packed attach-to-type:
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.module_net_variable_alignment = AlignmentPolicy::kAlign;
    style.module_net_variable_packed_dimensions =
        PackedDimensionsPlacement::kAttachToType;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input_packed, "<filename>", style, stream));
    EXPECT_EQ(stream.str(),
              "module m;\n"
              "  logic [31:0]            data;\n"
              "  some_really_long_type_t thing;\n"
              "endmodule\n");
  }

  const char input_unpacked[] =
      "module m;\n"
      "logic scoreboard_query_ready [OOO_INT_DISPATCH_QUERY_COUNT];\n"
      "int_phys_reg_idx_t scoreboard_query_tag "
      "[OOO_INT_DISPATCH_QUERY_COUNT];\n"
      "\n"
      "int_prf_read_request_t scheduled_prf_read_request "
      "[OOO_INT_PRF_READ_PORT_COUNT];\n"
      "int_prf_read_request_t prf_read_request [OOO_INT_PRF_READ_PORT_COUNT];\n"
      "xlen_t prf_read_value [OOO_INT_PRF_READ_PORT_COUNT];\n"
      "int_phys_reg_allocation_t prf_allocation [OOO_RENAME_WIDTH];\n"
      "endmodule\n";

  {
    // Unpacked attach-to-name + packed attach-to-type:
    FormatStyle style;
    style.column_limit = 120;
    style.indentation_spaces = 2;
    style.module_net_variable_alignment = AlignmentPolicy::kAlign;
    style.module_net_variable_packed_dimensions =
        PackedDimensionsPlacement::kAttachToType;
    style.module_net_variable_unpacked_dimensions =
        UnpackedDimensionsPlacement::kAttachToName;

    std::ostringstream stream;
    EXPECT_OK(FormatVerilog(input_unpacked, "<filename>", style, stream));
    const std::string formatted = stream.str();
    EXPECT_EQ(formatted,
              "module m;\n"
              "  logic                     "
              "scoreboard_query_ready [OOO_INT_DISPATCH_QUERY_COUNT];\n"
              "  int_phys_reg_idx_t        "
              "scoreboard_query_tag [OOO_INT_DISPATCH_QUERY_COUNT];\n"
              "\n"
              "  int_prf_read_request_t    "
              "scheduled_prf_read_request [OOO_INT_PRF_READ_PORT_COUNT];\n"
              "  int_prf_read_request_t    "
              "prf_read_request [OOO_INT_PRF_READ_PORT_COUNT];\n"
              "  xlen_t                    "
              "prf_read_value [OOO_INT_PRF_READ_PORT_COUNT];\n"
              "  int_phys_reg_allocation_t prf_allocation [OOO_RENAME_WIDTH];\n"
              "endmodule\n");

    // Verify idempotence
    std::ostringstream stream2;
    EXPECT_OK(FormatVerilog(formatted, "<filename>", style, stream2));
    EXPECT_EQ(stream2.str(), formatted);
  }
}

TEST(FormatterEndToEndTest, ModuleNetVariableMultipleUnpackedDimensionsTest) {
  const char input[] =
      "module m;\n"
      "foo_t foo [A][B];\n"
      "bar_type_t long_name [X][Y][Z];\n"
      "endmodule\n";

  FormatStyle style;
  style.column_limit = 120;
  style.indentation_spaces = 2;
  style.module_net_variable_alignment = AlignmentPolicy::kAlign;
  style.module_net_variable_packed_dimensions =
      PackedDimensionsPlacement::kAttachToType;
  style.module_net_variable_unpacked_dimensions =
      UnpackedDimensionsPlacement::kAttachToName;

  std::ostringstream stream;
  EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
  const std::string formatted = stream.str();
  EXPECT_EQ(formatted,
            "module m;\n"
            "  foo_t      foo [A][B];\n"
            "  bar_type_t long_name [X][Y][Z];\n"
            "endmodule\n");

  // Verify idempotence
  std::ostringstream stream2;
  EXPECT_OK(FormatVerilog(formatted, "<filename>", style, stream2));
  EXPECT_EQ(stream2.str(), formatted);
}

TEST(FormatterEndToEndTest, CombinedAcceptanceTest) {
  const char input[] =
      "module top (\n"
      "input logic clk,\n"
      "input logic rst_n,\n"
      "\n"
      "// Ports\n"
      "input logic [DATA_WIDTH-1:0] rx_data,\n"
      "output logic [DATA_WIDTH-1:0] tx_data,\n"
      "input my_pkg::header_t rx_header [DEPTH]\n"
      ");\n"
      "logic [31:0] internal_data;\n"
      "long_type_name_t internal_state;\n"
      "my_pkg::status_t status_array [PORTS];\n"
      "\n"
      "submodule u_sub (\n"
      ".clk(clk),\n"
      ".very_long_named_port(internal_data)\n"
      ");\n"
      "endmodule\n";

  FormatStyle style;
  style.column_limit = 120;
  style.indentation_spaces = 2;
  style.port_declarations_alignment = AlignmentPolicy::kAlign;
  style.port_declarations_indentation = IndentationStyle::kIndent;
  style.port_declarations_group_boundary = AlignmentGroupBoundary::kNone;
  style.port_declarations_packed_dimensions =
      PackedDimensionsPlacement::kAttachToType;
  style.port_declarations_unpacked_dimensions =
      UnpackedDimensionsPlacement::kAttachToName;

  style.module_net_variable_alignment = AlignmentPolicy::kAlign;
  style.module_net_variable_packed_dimensions =
      PackedDimensionsPlacement::kAttachToType;
  style.module_net_variable_unpacked_dimensions =
      UnpackedDimensionsPlacement::kAttachToName;

  style.named_port_alignment = AlignmentPolicy::kAlign;
  style.named_port_indentation = IndentationStyle::kIndent;
  style.named_port_minimum_spacing = 1;

  std::ostringstream stream;
  EXPECT_OK(FormatVerilog(input, "<filename>", style, stream));
  const std::string formatted = stream.str();
  EXPECT_EQ(formatted,
            "module top (\n"
            "  input  logic                  clk,\n"
            "  input  logic                  rst_n,\n"
            "\n"
            "  // Ports\n"
            "  input  logic [DATA_WIDTH-1:0] rx_data,\n"
            "  output logic [DATA_WIDTH-1:0] tx_data,\n"
            "  input  my_pkg::header_t       rx_header [DEPTH]\n"
            ");\n"
            "  logic [31:0]     internal_data;\n"
            "  long_type_name_t internal_state;\n"
            "  my_pkg::status_t status_array [PORTS];\n"
            "\n"
            "  submodule u_sub (\n"
            "    .clk                  (clk),\n"
            "    .very_long_named_port (internal_data)\n"
            "  );\n"
            "endmodule\n");

  // Verify idempotence
  std::ostringstream stream2;
  EXPECT_OK(FormatVerilog(formatted, "<filename>", style, stream2));
  EXPECT_EQ(stream2.str(), formatted);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
