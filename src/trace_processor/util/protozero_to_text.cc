#include "src/trace_processor/util/protozero_to_text.h"

#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/proto_decoder.h"
#include "perfetto/protozero/proto_utils.h"
#include "protos/perfetto/common/descriptor.pbzero.h"
#include "src/trace_processor/util/descriptors.h"

// This is the highest level that this protozero to text supports.
#include "src/trace_processor/importers/track_event.descriptor.h"

namespace perfetto {
namespace trace_processor {
namespace protozero_to_text {

namespace {

using protozero::proto_utils::ProtoWireType;
using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;

// This function matches the implementation of TextFormatEscaper.escapeBytes
// from the Java protobuf library.
std::string QuoteAndEscapeTextProtoString(const std::string& raw) {
  std::string ret;
  for (auto it = raw.cbegin(); it != raw.cend(); it++) {
    switch (*it) {
      case '\a':
        ret += "\\a";
        break;
      case '\b':
        ret += "\\b";
        break;
      case '\f':
        ret += "\\f";
        break;
      case '\n':
        ret += "\\n";
        break;
      case '\r':
        ret += "\\r";
        break;
      case '\t':
        ret += "\\t";
        break;
      case '\v':
        ret += "\\v";
        break;
      case '\\':
        ret += "\\\\";
        break;
      case '\'':
        ret += "\\\'";
        break;
      case '"':
        ret += "\\\"";
        break;
      default:
        // Only ASCII characters between 0x20 (space) and 0x7e (tilde) are
        // printable; other byte values are escaped with 3-character octal
        // codes.
        if (*it >= 0x20 && *it <= 0x7e) {
          ret += *it;
        } else {
          ret += '\\';

          // Cast to unsigned char to make the right shift unsigned as well.
          unsigned char c = static_cast<unsigned char>(*it);
          ret += ('0' + ((c >> 6) & 3));
          ret += ('0' + ((c >> 3) & 7));
          ret += ('0' + (c & 7));
        }
        break;
    }
  }
  return '"' + ret + '"';
}

// Recursively determine the size of all the string like things passed in the
// parameter pack |rest|.
size_t SizeOfStr() {
  return 0;
}
template <typename T, typename... Rest>
size_t SizeOfStr(const T& first, Rest... rest) {
  return base::StringView(first).size() + SizeOfStr(rest...);
}

// Append |to_add| which is something string like to |out|.
template <typename T>
void StrAppendInternal(std::string* out, const T& to_add) {
  out->append(to_add);
}

template <typename T, typename... strings>
void StrAppendInternal(std::string* out, const T& first, strings... values) {
  StrAppendInternal(out, first);
  StrAppendInternal(out, values...);
}

// Append |to_add| which is something string like to |out|.
template <typename T>
void StrAppend(std::string* out, const T& to_add) {
  out->reserve(out->size() + base::StringView(to_add).size());
  out->append(to_add);
}

template <typename T, typename... strings>
void StrAppend(std::string* out, const T& first, strings... values) {
  out->reserve(out->size() + SizeOfStr(values...));
  StrAppendInternal(out, first);
  StrAppendInternal(out, values...);
}

void IncreaseIndents(std::string* out) {
  StrAppend(out, "  ");
}

void DecreaseIndents(std::string* out) {
  PERFETTO_DCHECK(out->size() >= 2);
  out->erase(out->size() - 2);
}

std::string FormattedFieldDescriptorName(
    const FieldDescriptor& field_descriptor) {
  if (field_descriptor.is_extension()) {
    // Libprotobuf formatter always formats extension field names as fully
    // qualified names.
    // TODO(b/197625974): Assuming for now all our extensions will belong to the
    // perfetto.protos package. Update this if we ever want to support extendees
    // in different package.
    return "[perfetto.protos." + field_descriptor.name() + "]";
  } else {
    return field_descriptor.name();
  }
}

void PrintVarIntField(const FieldDescriptor* fd,
                      const protozero::Field& field,
                      const DescriptorPool& pool,
                      std::string* out) {
  uint32_t type = fd ? fd->type() : 0;
  switch (type) {
    case FieldDescriptorProto::TYPE_INT32:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_int32()));
      return;
    case FieldDescriptorProto::TYPE_SINT32:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_sint32()));
      return;
    case FieldDescriptorProto::TYPE_UINT32:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_uint32()));
      return;
    case FieldDescriptorProto::TYPE_INT64:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_int64()));
      return;
    case FieldDescriptorProto::TYPE_SINT64:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_sint64()));
      return;
    case FieldDescriptorProto::TYPE_UINT64:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_uint64()));
      return;
    case FieldDescriptorProto::TYPE_BOOL:
      StrAppend(out, fd->name(), ": ", field.as_bool() ? "true" : "false");
      return;
    case FieldDescriptorProto::TYPE_ENUM: {
      // If the enum value is unknown, treat it like a completely unknown field.
      auto opt_enum_descriptor_idx =
          pool.FindDescriptorIdx(fd->resolved_type_name());
      if (!opt_enum_descriptor_idx)
        break;
      auto opt_enum_string =
          pool.descriptors()[*opt_enum_descriptor_idx].FindEnumString(
              field.as_int32());
      if (!opt_enum_string)
        break;
      StrAppend(out, fd->name(), ": ", *opt_enum_string);
      return;
    }
    case 0:
    default:
      break;
  }
  StrAppend(out, std::to_string(field.id()), ": ",
            std::to_string(field.as_uint64()));
}

void PrintFixed32Field(const FieldDescriptor* fd,
                       const protozero::Field& field,
                       std::string* out) {
  uint32_t type = fd ? fd->type() : 0;
  switch (type) {
    case FieldDescriptorProto::TYPE_SFIXED32:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_int32()));
      break;
    case FieldDescriptorProto::TYPE_FIXED32:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_uint32()));
      break;
    case FieldDescriptorProto::TYPE_FLOAT:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_float()));
      break;
    case 0:
    default:
      base::StackString<12> padded_hex("0x%08" PRIx32, field.as_uint32());
      StrAppend(out, std::to_string(field.id()), ": ", padded_hex.c_str());
      break;
  }
}

void PrintFixed64Field(const FieldDescriptor* fd,
                       const protozero::Field& field,
                       std::string* out) {
  uint32_t type = fd ? fd->type() : 0;
  switch (type) {
    case FieldDescriptorProto::TYPE_SFIXED64:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_int64()));
      break;
    case FieldDescriptorProto::TYPE_FIXED64:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_uint64()));
      break;
    case FieldDescriptorProto::TYPE_DOUBLE:
      StrAppend(out, fd->name(), ": ", std::to_string(field.as_double()));
      break;
    case 0:
    default:
      base::StackString<20> padded_hex("0x%016" PRIx64, field.as_uint64());
      StrAppend(out, std::to_string(field.id()), ": ", padded_hex.c_str());
      break;
  }
}

void ProtozeroToTextInternal(const std::string& type,
                             protozero::ConstBytes protobytes,
                             NewLinesMode new_lines_mode,
                             const DescriptorPool& pool,
                             std::string* indents,
                             std::string* output);

void PrintLengthDelimitedField(const FieldDescriptor* fd,
                               const protozero::Field& field,
                               NewLinesMode new_lines_mode,
                               std::string* indents,
                               const DescriptorPool& pool,
                               std::string* out) {
  const bool include_new_lines = new_lines_mode == kIncludeNewLines;
  uint32_t type = fd ? fd->type() : 0;
  switch (type) {
    case FieldDescriptorProto::TYPE_BYTES:
    case FieldDescriptorProto::TYPE_STRING: {
      std::string value = QuoteAndEscapeTextProtoString(field.as_std_string());
      StrAppend(out, fd->name(), ": ", value);
      break;
    }
    case FieldDescriptorProto::TYPE_MESSAGE:
      StrAppend(out, FormattedFieldDescriptorName(*fd), ": {");
      if (include_new_lines) {
        IncreaseIndents(indents);
      }
      ProtozeroToTextInternal(fd->resolved_type_name(), field.as_bytes(),
                              new_lines_mode, pool, indents, out);
      if (include_new_lines) {
        DecreaseIndents(indents);
        StrAppend(out, "\n", *indents, "}");
      } else {
        StrAppend(out, " }");
      }
      break;
    case 0:
    default:
      std::string value = QuoteAndEscapeTextProtoString(field.as_std_string());
      StrAppend(out, std::to_string(field.id()), ": ", value);
      break;
  }
}

// Recursive case function, Will parse |protobytes| assuming it is a proto of
// |type| and will use |pool| to look up the |type|. All output will be placed
// in |output|, using |new_lines_mode| to separate fields. When called for
// |indents| will be increased by 2 spaces to improve readability.
void ProtozeroToTextInternal(const std::string& type,
                             protozero::ConstBytes protobytes,
                             NewLinesMode new_lines_mode,
                             const DescriptorPool& pool,
                             std::string* indents,
                             std::string* output) {
  auto opt_proto_descriptor_idx = pool.FindDescriptorIdx(type);
  PERFETTO_DCHECK(opt_proto_descriptor_idx);
  auto& proto_descriptor = pool.descriptors()[*opt_proto_descriptor_idx];
  const bool include_new_lines = new_lines_mode == kIncludeNewLines;

  protozero::ProtoDecoder decoder(protobytes.data, protobytes.size);
  for (auto field = decoder.ReadField(); field.valid();
       field = decoder.ReadField()) {
    if (!output->empty()) {
      if (include_new_lines) {
        StrAppend(output, "\n", *indents);
      } else {
        StrAppend(output, " ", *indents);
      }
    } else {
      StrAppend(output, *indents);
    }
    auto* opt_field_descriptor = proto_descriptor.FindFieldByTag(field.id());
    switch (field.type()) {
      case ProtoWireType::kVarInt:
        PrintVarIntField(opt_field_descriptor, field, pool, output);
        break;
      case ProtoWireType::kLengthDelimited:
        PrintLengthDelimitedField(opt_field_descriptor, field, new_lines_mode,
                                  indents, pool, output);
        break;
      case ProtoWireType::kFixed32:
        PrintFixed32Field(opt_field_descriptor, field, output);
        break;
      case ProtoWireType::kFixed64:
        PrintFixed64Field(opt_field_descriptor, field, output);
        break;
    }
  }
  PERFETTO_DCHECK(decoder.bytes_left() == 0);
}

}  // namespace

std::string ProtozeroToText(const DescriptorPool& pool,
                            const std::string& type,
                            protozero::ConstBytes protobytes,
                            NewLinesMode new_lines_mode,
                            uint32_t initial_indent_depth) {
  std::string indent = std::string(2 * initial_indent_depth, ' ');
  std::string final_result;
  ProtozeroToTextInternal(type, protobytes, new_lines_mode, pool, &indent,
                          &final_result);
  return final_result;
}

std::string DebugTrackEventProtozeroToText(const std::string& type,
                                           protozero::ConstBytes protobytes) {
  DescriptorPool pool;
  auto status = pool.AddFromFileDescriptorSet(kTrackEventDescriptor.data(),
                                              kTrackEventDescriptor.size());
  PERFETTO_DCHECK(status.ok());
  return ProtozeroToText(pool, type, protobytes, kIncludeNewLines);
}

std::string ShortDebugTrackEventProtozeroToText(
    const std::string& type,
    protozero::ConstBytes protobytes) {
  DescriptorPool pool;
  auto status = pool.AddFromFileDescriptorSet(kTrackEventDescriptor.data(),
                                              kTrackEventDescriptor.size());
  PERFETTO_DCHECK(status.ok());
  return ProtozeroToText(pool, type, protobytes, kSkipNewLines);
}

std::string ProtozeroEnumToText(const std::string& type, int32_t enum_value) {
  DescriptorPool pool;
  auto status = pool.AddFromFileDescriptorSet(kTrackEventDescriptor.data(),
                                              kTrackEventDescriptor.size());
  PERFETTO_DCHECK(status.ok());
  auto opt_enum_descriptor_idx = pool.FindDescriptorIdx(type);
  if (!opt_enum_descriptor_idx) {
    // Fall back to the integer representation of the field.
    return std::to_string(enum_value);
  }
  auto opt_enum_string =
      pool.descriptors()[*opt_enum_descriptor_idx].FindEnumString(enum_value);
  if (!opt_enum_string) {
    // Fall back to the integer representation of the field.
    return std::to_string(enum_value);
  }
  return *opt_enum_string;
}

std::string ProtozeroToText(const DescriptorPool& pool,
                            const std::string& type,
                            const std::vector<uint8_t>& protobytes,
                            NewLinesMode new_lines_mode) {
  return ProtozeroToText(
      pool, type, protozero::ConstBytes{protobytes.data(), protobytes.size()},
      new_lines_mode);
}

}  // namespace protozero_to_text
}  // namespace trace_processor
}  // namespace perfetto
