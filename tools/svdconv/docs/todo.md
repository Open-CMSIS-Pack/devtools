# SVDConv TODO

## Field ranges and 32-bit output

- [ ] Review whether a checked `SvdField::GetBitRange(uint32_t&, uint32_t&)` accessor should own the validation
      currently provided by `SvdUtils::GetBitRange32()`. This is a possible model cleanup; the current helper
      preserves the existing raw getter contracts.

`SvdUtils::GetBitRange32()` in [SvdUtils.cpp](../SVDModel/src/SvdUtils.cpp) validates the complete field range
before converting model values to the 32-bit values used by the generators:

- `GetBitWidth()` already returns `int32_t`; `-1` represents an unset value used during inheritance.
- `SvdField::GetOffset()` retains a `uint64_t` raw value, including its unset sentinel and invalid inputs needed
  for inheritance and diagnostics. Narrowing this getter could turn an invalid offset such as `0x100000000`
  into the valid position `0` before validation.
- Offset and width must be checked together: offset `31` and width `2` exceed the supported 32-bit range.

The helper checks offset, width and their combined range before either conversion. It leaves the output
parameters unchanged on failure. Existing wide model values and intermediate arithmetic do not imply support
for 64-bit SVD input or output.

Changing the raw getters or field storage requires coordinated changes to parsing, setters, dimension
calculations, inheritance and sentinel handling. Preserve invalid raw values until diagnostics can report them,
and retain the regression coverage in [SvdSentinelModelTest.cpp](../test/unittests/src/SvdSentinelModelTest.cpp)
and [SvdBitWidthTest.cpp](../test/unittests/src/SvdBitWidthTest.cpp), including full-width masks, bit 31,
negative widths and large offsets that would otherwise wrap.
