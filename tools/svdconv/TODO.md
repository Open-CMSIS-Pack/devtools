# SVDConv TODO

This file collects findings from a source, generator, diagnostic, and test audit of SVDConv. The items are
grouped by topic and should be addressed in focused pull requests rather than as one combined change.

Relevant references:

- [CMSIS-SVD register and field elements](https://open-cmsis-pack.github.io/svd-spec/latest/elem_registers.html)
- [CMSIS-SVD special elements](https://open-cmsis-pack.github.io/svd-spec/latest/elem_special.html)
- [CMSIS-SVD schema](https://github.com/Open-CMSIS-Pack/svd-spec/blob/main/schema/CMSIS-SVD.xsd)

## High-priority correctness

- [ ] Make 64-bit register and field handling consistent.
  - Decide whether SVDConv supports 64-bit registers and fields throughout or rejects them during validation.
  - Remove shifts by the operand width in `SvdField.cpp`, `SvdRegister.cpp`, `SfdData_SingleItems.cpp`, and
    `HeaderData_PosMask.cpp`.
  - Replace the signed 32-bit mask calculation in `HeaderData_Field.cpp`.
  - Preserve 64-bit enumerated values instead of reading or copying only `Value::u32`.
  - Cover checking, SFD generation, and header generation with sanitizer-enabled tests. The existing
    `ResetMask.svd` already exercises a 64-bit register and field.

- [ ] Reject unknown SAU region access values.
  - `SvdUtils::ConvertSauAccessType` currently returns success for values other than `c` and `n`.
  - Prevent an undefined access type from being emitted as Non-Secure by `PartitionData`.
  - Add parser and partition-generation tests for valid and invalid values.

- [ ] Remove `selectable` from `modifiedWriteValues` parsing.
  - It is a CPU endian value, but `SvdUtils::ConvertModifiedWriteValues` currently accepts it and maps it to `set`.
  - Report a parse error for every value outside the CMSIS-SVD `modifiedWriteValuesType` set.

- [ ] Implement or explicitly reject `writeConstraint`.
  - `writeAsRead`, `useEnumeratedValues`, and `range` are currently accepted but discarded.
  - Store and validate the selected, mutually exclusive constraint.
  - Parse and validate `range/minimum` and `range/maximum`, including ordering and fit within the register or
    field width.
  - Define inheritance from registers to fields and copying through `derivedFrom`.
  - Validate `useEnumeratedValues` against the applicable write enumeration.
  - Fix ownership of the register-level `SvdWriteConstraint` object.

## Enumerated values

- [ ] Complete `<isDefault>` handling.
  - Detect more than one default entry in an `enumeratedValues` container instead of silently using the last one.
  - Reconstruct or copy `SvdEnumContainer::m_defaultValue` for `derivedFrom` enumerations.
  - Make SFD descriptions use the default entry for unspecified values, matching combo generation.
  - Ensure default entries do not participate as an implicit numeric value zero in descriptions or duplicate checks.
  - Add direct, derived, duplicate-default, and no-default tests.

- [ ] Select the correct enumeration container for SFD controls.
  - `SfdData::CreateEnumValues` currently uses the first container when separate `read` and `write` containers exist.
  - Define which mapping represents an editable control and which mapping is used for read-only display.
  - Do not base combo generation or M227 solely on whether the first container has children.
  - Apply the same selection rule to the generated value description.

## Numeric and model validation

- [ ] Align numeric parsing with the CMSIS-SVD numeric types.
  - Detect `uint64_t` overflow during multiplication and addition.
  - Reject overflow when converting to `uint32_t` or `int32_t` instead of truncating.
  - Support or deliberately reject with a clear diagnostic the leading `+` and `k`, `m`, `g`, and `t` suffixes
    accepted by `scaledNonNegativeInteger`.
  - Accept boolean text only for boolean elements, not through the general numeric parser.
  - Add boundary tests for every supported base and destination width.

- [ ] Correct `SvdRegister::GetAccessCalculated`.
  - Include each field's bit offset when accumulating covered register bits.
  - Handle full-width masks without shifting by 64.
  - Add tests with multiple disjoint fields, uncovered bits, and mixed field access types.

- [ ] Correct register-array size validation.
  - Use the device's `addressUnitBits` consistently instead of `dimIncrement * 8` in the exact-size check.
  - Pass the calculated bit increment rather than the unscaled `dimIncrement` to M366.
  - Add coverage for devices whose addressable unit is not eight bits.

- [ ] Define and consistently enforce handling of unknown XML content.
  - Replace the process-wide static ten-message limit for unknown elements with per-input accounting and a
    suppression summary, or remove the limit.
  - Decide whether unknown attributes are accepted for forward compatibility or diagnosed; they are currently
    ignored silently.
  - Ensure the behavior is explicit because SVDConv performs model validation rather than XSD validation.

## Diagnostics and documentation

- [ ] Align M302 with the register widths accepted by the validator, including the decision about 64-bit support.
- [ ] Resolve whether the valid M384 PMU event-counter range ends at 31 or 32, then align the condition and message.
- [ ] Change M389 from "greater or equal" to "greater than" if equality remains accepted.
- [ ] Make M366 report the address increment in bits used by the validation.
- [ ] Add the missing closing apostrophes to M221 and M222.
- [ ] Correct "BitWith" to "bit width" in M313.
- [ ] Synchronize `SVDConv/README.md` with the active message table.
  - Correct the documented severity of M307.
  - Document M244 through M248 and M383 through M391.
  - Remove the duplicated text in M108.

## Tests and quality checks

- [ ] Add focused regression tests for every correctness item above.
  - Missing coverage includes `isDefault`, `writeConstraint`, invalid SAU access, invalid `modifiedWriteValues`,
    numeric overflow, scaled numbers, and read/write enumeration selection.
- [ ] Enable and repair `SvdUtilsUnitTests.DISABLED_CheckTextGeneric_SfrCC2_escape`.
- [ ] Decide how CP1252 input is handled, then enable or replace
  `SvdUtilsUnitTests.DISABLED_CheckTextGeneric_SfrCC2_CP1252_doubleQuote`.
- [ ] Replace the empty `CodeGenerator.Check` test body with an active test or remove the test.
- [ ] Restore the disabled `CheckDisableCondition` integration test; its current unconditional success does not
  test the feature.
- [ ] Add an UndefinedBehaviorSanitizer test configuration for SVDConv model and generator paths.

## Additional robustness review

- [ ] Preserve failures from `SvdItem::ProcessXmlAttributes`; `SvdItem::Construct` currently overwrites that
  result with the child-processing result.
- [ ] Verify that an XML `ParseAll` failure stops model construction, validation, and generation instead of
  continuing with a partial tree.
- [ ] Propagate output create, write, and flush failures so an incomplete generated file cannot appear successful.
