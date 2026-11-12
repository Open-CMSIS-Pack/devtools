/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include "common/ocsd_dcd_tree.h"
#include "common/trc_gen_elem.h"
#include "interfaces/trc_gen_elem_in_i.h"
#include "interfaces/trc_tgt_mem_access_i.h"
#include "opencsd/etmv3/trc_cmp_cfg_etmv3.h"
#include "opencsd/etmv4/trc_cmp_cfg_etmv4.h"
#include "opencsd/ptm/trc_cmp_cfg_ptm.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

/** @brief Exercises the dependency's instruction reads with an exact, unpadded code image. */
class OpenCsdInstructionBoundaryDecoder final : public ITrcGenElemIn, public ITargetMemAccess {
public:
  enum class Protocol { Etmv3, Etmv4, Etmv4Q, Ptm };
  enum class InstructionSet { Thumb, Arm, AArch64 };

  explicit OpenCsdInstructionBoundaryDecoder(Protocol protocol, std::vector<std::uint8_t> image,
                                             InstructionSet instructionSet = InstructionSet::Thumb)
    : m_image(std::move(image)),
      m_tree(DecodeTree::CreateDecodeTree(OCSD_TRC_SRC_SINGLE, 0U), DecodeTree::DestroyDecodeTree)
  {
    require(m_tree != nullptr, "create instruction-boundary decode tree");
    m_tree->setGenTraceElemOutI(this);
    m_tree->setMemAccessI(this);
    createDecoder(protocol, instructionSet);
    const auto raw = traceBytes(protocol, instructionSet);
    std::uint32_t processed = 0U;
    checkResponse(m_tree->TraceDataIn(OCSD_OP_DATA, 0U, static_cast<std::uint32_t>(raw.size()), raw.data(), &processed));
    require(processed == raw.size(), "consume complete instruction-boundary trace");
    checkResponse(m_tree->TraceDataIn(OCSD_OP_EOT, raw.size(), 0U, nullptr, nullptr));
  }

  /** @brief Copies generic output while the callback-owned element is available. */
  ocsd_datapath_resp_t TraceElemIn(ocsd_trc_index_t, std::uint8_t, const OcsdTraceElement& element) override
  {
    m_elements.push_back(element);
    return OCSD_RESP_CONT;
  }

  /** @brief Returns only the bytes present in the image, including short reads. */
  ocsd_err_t ReadTargetMemory(ocsd_vaddr_t address, std::uint8_t, ocsd_mem_space_acc_t,
                            std::uint32_t* size, std::uint8_t* output) override
  {
    if (address < CodeAddress || address - CodeAddress >= m_image.size()) {
      *size = 0U;
    } else {
      const auto offset = static_cast<std::size_t>(address - CodeAddress);
      *size = static_cast<std::uint32_t>(std::min<std::size_t>(*size, m_image.size() - offset));
      std::copy_n(m_image.data() + offset, *size, output);
    }
    return OCSD_OK;
  }

  /** @brief The test image is immutable and has no memory cache. */
  void InvalidateMemAccCache(std::uint8_t) override {}

  std::vector<OcsdTraceElement> elementsOfType(ocsd_gen_trc_elem_t type) const
  {
    std::vector<OcsdTraceElement> selected;
    for (const auto& element : m_elements) {
      if (element.getType() == type) {
        selected.push_back(element);
      }
    }
    return selected;
  }

  static constexpr ocsd_vaddr_t CodeAddress = 0x1000U;

private:
  void createDecoder(Protocol protocol, InstructionSet instructionSet)
  {
    if (protocol == Protocol::Etmv3) {
      ocsd_etmv3_cfg registers{};
      registers.arch_ver = ARCH_V7;
      registers.core_prof = instructionSet == InstructionSet::Thumb ? profile_CortexM : profile_CortexA;
      registers.reg_idr = 0x4114f250U;
      registers.reg_ctrl = 0x810U;
      registers.reg_ccer = 0x18541800U;
      registers.reg_trc_id = 6U;
      const EtmV3Config config(&registers);
      require(m_tree->createDecoder(OCSD_BUILTIN_DCD_ETMV3, OCSD_CREATE_FLG_FULL_DECODER, &config) == OCSD_OK,
              "create ETMv3 instruction-boundary decoder");
    } else if (protocol == Protocol::Ptm) {
      ocsd_ptm_cfg registers{};
      registers.arch_ver = ARCH_V7;
      registers.core_prof = profile_CortexA;
      registers.reg_idr = 0x4100f310U;
      registers.reg_trc_id = 6U;
      const PtmConfig config(&registers);
      require(m_tree->createDecoder(OCSD_BUILTIN_DCD_PTM, OCSD_CREATE_FLG_FULL_DECODER, &config) == OCSD_OK,
              "create PTM instruction-boundary decoder");
    } else {
      ocsd_etmv4_cfg registers{};
      registers.arch_ver = instructionSet == InstructionSet::AArch64 ? ARCH_V8 : ARCH_V7;
      registers.core_prof = instructionSet == InstructionSet::Thumb ? profile_CortexM : profile_CortexA;
      registers.reg_idr0 = protocol == Protocol::Etmv4Q ? 0x080186e1U : 0x080006e1U;
      registers.reg_idr1 = 0x4100f401U;
      registers.reg_idr2 = instructionSet == InstructionSet::AArch64 ? 8U : 4U;
      registers.reg_idr8 = 1U;
      registers.reg_configr = 1U;
      registers.reg_traceidr = 6U;
      const EtmV4Config config(&registers);
      require(m_tree->createDecoder(OCSD_BUILTIN_DCD_ETMV4I, OCSD_CREATE_FLG_FULL_DECODER, &config) == OCSD_OK,
              "create ETMv4 instruction-boundary decoder");
    }
  }

  static std::vector<std::uint8_t> traceBytes(Protocol protocol, InstructionSet instructionSet)
  {
    const std::uint8_t thumbBit = instructionSet == InstructionSet::Thumb ? 1U : 0U;
    if (protocol == Protocol::Ptm) {
      // A-Sync; I-Sync at 0x1000; one executed waypoint.
      return {0U, 0U, 0U, 0U, 0U, 0x80U, 0x08U, thumbBit, 0x10U, 0U, 0U, 0x20U, 0x80U};
    }
    if (protocol == Protocol::Etmv3) {
      // A-Sync; I-Sync at 0x1000; one executed instruction.
      return {0U, 0U, 0U, 0U, 0U, 0x80U, 0x08U, 0x20U, thumbBit, 0x10U, 0U, 0U, 0x84U};
    }
    // A-Sync; Trace Info; address and context at 0x1000.
    std::vector<std::uint8_t> raw = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0x80U,
        0x01U, 0x01U, 0U};
    if (instructionSet == InstructionSet::Thumb) {
      raw.insert(raw.end(), {0x83U, 0U, 0x10U, 0U, 0U, 0x03U});
    } else if (instructionSet == InstructionSet::Arm) {
      raw.insert(raw.end(), {0x82U, 0U, 0x08U, 0U, 0U, 0x03U});
    } else {
      raw.insert(raw.end(), {0x85U, 0U, 0x08U, 0U, 0U, 0U, 0U, 0U, 0U, 0x13U});
    }
    if (protocol == Protocol::Etmv4Q) {
      // Q with one instruction and continuation address 0x1000.
      raw.insert(raw.end(), {0xabU, 0U, 0x10U, 0U, 0U, 0x01U});
    } else {
      raw.push_back(0xf7U); // One executed branch atom.
    }
    raw.insert(raw.end(), {0x2dU, 0x01U}); // Explicitly commit the atom or Q element.
    return raw;
  }

  void checkResponse(ocsd_datapath_resp_t response)
  {
    for (unsigned attempt = 0U; OCSD_DATA_RESP_IS_WAIT(response) && attempt < 8U; ++attempt) {
      response = m_tree->TraceDataIn(OCSD_OP_FLUSH, 0U, 0U, nullptr, nullptr);
    }
    require(!OCSD_DATA_RESP_IS_FATAL(response) && !OCSD_DATA_RESP_IS_WAIT(response),
            "complete instruction-boundary decoder operation");
  }

  std::vector<std::uint8_t> m_image;
  std::vector<OcsdTraceElement> m_elements;
  std::unique_ptr<DecodeTree, decltype(&DecodeTree::DestroyDecodeTree)> m_tree;
};

class OpenCsdInstructionBoundaryChecks {
public:
  using Decoder = OpenCsdInstructionBoundaryDecoder;

  static void expectCompleteThumb16AtImageEnd(Decoder::Protocol protocol)
  {
    expectInstruction(protocol, {0xfeU, 0xe7U}, 2U); // B 0x1000.
  }

  static void expectCompleteThumb16WithOneTrailingByte(Decoder::Protocol protocol)
  {
    expectInstruction(protocol, {0xfeU, 0xe7U, 0xffU}, 2U);
  }

  static void expectMissingOrPartialFirstHalfword(Decoder::Protocol protocol)
  {
    expectUnavailable(protocol, {});
    expectUnavailable(protocol, {0xfeU});
  }

  static void expectIncompleteThumb32ForEveryWidePrefix(Decoder::Protocol protocol)
  {
    for (const auto prefix : {0xe8U, 0xf0U, 0xf8U}) {
      SCOPED_TRACE(prefix);
      expectUnavailable(protocol, {0U, static_cast<std::uint8_t>(prefix)});
      expectUnavailable(protocol, {0U, static_cast<std::uint8_t>(prefix), 0xffU});
    }
  }

  static void expectCompleteThumb32AtImageEnd(Decoder::Protocol protocol)
  {
    expectInstruction(protocol, {0xffU, 0xf7U, 0xfeU, 0xbfU}, 4U); // B.W 0x1000.
  }

private:
  static void expectInstruction(Decoder::Protocol protocol, const std::vector<std::uint8_t>& image, std::uint32_t width)
  {
    const Decoder decoder(protocol, image);
    const auto ranges = decoder.elementsOfType(OCSD_GEN_TRC_ELEM_INSTR_RANGE);
    ASSERT_EQ(ranges.size(), 1U);
    EXPECT_EQ(ranges.front().st_addr, Decoder::CodeAddress);
    EXPECT_EQ(ranges.front().en_addr, Decoder::CodeAddress + width);
    EXPECT_EQ(ranges.front().num_instr_range, 1U);
    EXPECT_EQ(ranges.front().last_instr_sz, width);
    EXPECT_TRUE(decoder.elementsOfType(OCSD_GEN_TRC_ELEM_ADDR_NACC).empty());
    EXPECT_TRUE(decoder.elementsOfType(OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH).empty());
  }

  static void expectUnavailable(Decoder::Protocol protocol, const std::vector<std::uint8_t>& image)
  {
    const Decoder decoder(protocol, image);
    EXPECT_TRUE(decoder.elementsOfType(OCSD_GEN_TRC_ELEM_INSTR_RANGE).empty());
    const auto type = protocol == Decoder::Protocol::Etmv4Q ? OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH
                                                          : OCSD_GEN_TRC_ELEM_ADDR_NACC;
    const auto unavailable = decoder.elementsOfType(type);
    ASSERT_EQ(unavailable.size(), 1U);
    EXPECT_EQ(unavailable.front().st_addr, Decoder::CodeAddress);
  }
};

TEST(CtraceUnitTests, testOpenCsdEtmv3DecodesCompleteThumb16AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv3);
}

TEST(CtraceUnitTests, testOpenCsdEtmv3DecodesCompleteThumb16WithOneTrailingByte)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16WithOneTrailingByte(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv3);
}

TEST(CtraceUnitTests, testOpenCsdEtmv3RejectsMissingOrPartialFirstHalfword)
{
  OpenCsdInstructionBoundaryChecks::expectMissingOrPartialFirstHalfword(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv3);
}

TEST(CtraceUnitTests, testOpenCsdEtmv3RejectsIncompleteThumb32ForEveryWidePrefix)
{
  OpenCsdInstructionBoundaryChecks::expectIncompleteThumb32ForEveryWidePrefix(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv3);
}

TEST(CtraceUnitTests, testOpenCsdEtmv3DecodesCompleteThumb32AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb32AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv3);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4DecodesCompleteThumb16AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4DecodesCompleteThumb16WithOneTrailingByte)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16WithOneTrailingByte(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4RejectsMissingOrPartialFirstHalfword)
{
  OpenCsdInstructionBoundaryChecks::expectMissingOrPartialFirstHalfword(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4RejectsIncompleteThumb32ForEveryWidePrefix)
{
  OpenCsdInstructionBoundaryChecks::expectIncompleteThumb32ForEveryWidePrefix(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4DecodesCompleteThumb32AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb32AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4QDecodesCompleteThumb16AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4Q);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4QDecodesCompleteThumb16WithOneTrailingByte)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16WithOneTrailingByte(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4Q);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4QRejectsMissingOrPartialFirstHalfword)
{
  OpenCsdInstructionBoundaryChecks::expectMissingOrPartialFirstHalfword(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4Q);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4QRejectsIncompleteThumb32ForEveryWidePrefix)
{
  OpenCsdInstructionBoundaryChecks::expectIncompleteThumb32ForEveryWidePrefix(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4Q);
}

TEST(CtraceUnitTests, testOpenCsdEtmv4QDecodesCompleteThumb32AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb32AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Etmv4Q);
}

TEST(CtraceUnitTests, testOpenCsdPtmDecodesCompleteThumb16AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Ptm);
}

TEST(CtraceUnitTests, testOpenCsdPtmDecodesCompleteThumb16WithOneTrailingByte)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb16WithOneTrailingByte(
      OpenCsdInstructionBoundaryDecoder::Protocol::Ptm);
}

TEST(CtraceUnitTests, testOpenCsdPtmRejectsMissingOrPartialFirstHalfword)
{
  OpenCsdInstructionBoundaryChecks::expectMissingOrPartialFirstHalfword(
      OpenCsdInstructionBoundaryDecoder::Protocol::Ptm);
}

TEST(CtraceUnitTests, testOpenCsdPtmRejectsIncompleteThumb32ForEveryWidePrefix)
{
  OpenCsdInstructionBoundaryChecks::expectIncompleteThumb32ForEveryWidePrefix(
      OpenCsdInstructionBoundaryDecoder::Protocol::Ptm);
}

TEST(CtraceUnitTests, testOpenCsdPtmDecodesCompleteThumb32AtImageEnd)
{
  OpenCsdInstructionBoundaryChecks::expectCompleteThumb32AtImageEnd(
      OpenCsdInstructionBoundaryDecoder::Protocol::Ptm);
}

TEST(CtraceUnitTests, testOpenCsdFixedWidthInstructionsStillRequireFourBytes)
{
  using Decoder = OpenCsdInstructionBoundaryDecoder;
  for (const auto& [protocol, instructionSet] : {
           std::pair{Decoder::Protocol::Etmv3, Decoder::InstructionSet::Arm},
           std::pair{Decoder::Protocol::Etmv4, Decoder::InstructionSet::Arm},
           std::pair{Decoder::Protocol::Ptm, Decoder::InstructionSet::Arm},
           std::pair{Decoder::Protocol::Etmv4, Decoder::InstructionSet::AArch64}}) {
    SCOPED_TRACE(static_cast<int>(protocol));
    SCOPED_TRACE(static_cast<int>(instructionSet));
    for (const auto& bytes : {std::vector<std::uint8_t>{0xfeU, 0xe7U},
                             std::vector<std::uint8_t>{0xfeU, 0xe7U, 0xffU}}) {
      const Decoder decoder(protocol, bytes, instructionSet);
      EXPECT_TRUE(decoder.elementsOfType(OCSD_GEN_TRC_ELEM_INSTR_RANGE).empty());
      const auto missing = decoder.elementsOfType(OCSD_GEN_TRC_ELEM_ADDR_NACC);
      ASSERT_EQ(missing.size(), 1U);
      EXPECT_EQ(missing.front().st_addr, Decoder::CodeAddress);
    }
    const auto branch = instructionSet == Decoder::InstructionSet::Arm
                            ? std::vector<std::uint8_t>{0xfeU, 0xffU, 0xffU, 0xeaU}
                            : std::vector<std::uint8_t>{0U, 0U, 0U, 0x14U};
    const Decoder decoder(protocol, branch, instructionSet);
    const auto ranges = decoder.elementsOfType(OCSD_GEN_TRC_ELEM_INSTR_RANGE);
    ASSERT_EQ(ranges.size(), 1U);
    EXPECT_EQ(ranges.front().st_addr, Decoder::CodeAddress);
    EXPECT_EQ(ranges.front().en_addr, Decoder::CodeAddress + 4U);
    EXPECT_EQ(ranges.front().last_instr_sz, 4U);
  }
}
