#include "aves_string.h"
#include "ov_string.h"
#include "ov_stringbuffer.h"
#include "ov_unicode.h"

namespace error_strings
{
	LitString<36> _IndexOutOfRange    = LitString<36>::FromCString("String character index out of range.");
	LitString<55> _FormatValueType    = LitString<55>::FromCString("The argument to String.format must be a List or a Hash.");
	LitString<62> _ReplaceEmptyString = LitString<62>::FromCString("The oldValue in a replacement cannot be the empty string (\"\").");

	String *IndexOutOfRange    = _S(_IndexOutOfRange);
	String *FormatValueType    = _S(_FormatValueType);
	String *ReplaceEmptyString = _S(_ReplaceEmptyString);
}

int32_t GetIndex(ThreadHandle thread, String *str, Value *arg)
{
	IntFromValue(thread, arg);
	int64_t index = arg->integer;
	if (index < 0 || index >= str->length)
	{
		VM_PushString(thread, error_strings::IndexOutOfRange);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	return (int32_t)index;
}

AVES_API NATIVE_FUNCTION(aves_String_get_item)
{
	String *str = THISV.common.string;

	int32_t index = GetIndex(thread, str, args + 1);

	String *output = GC_ConstructString(thread, 1, &str->firstChar + index);
	VM_PushString(thread, output);
}

AVES_API NATIVE_FUNCTION(aves_String_get_length)
{
	VM_PushInt(thread, THISV.common.string->length);
}

AVES_API NATIVE_FUNCTION(aves_String_equalsIgnoreCase)
{
	if (!IsString(args[1]))
	{
		VM_PushBool(thread, false);
	}
	else
	{
		bool result = String_EqualsIgnoreCase(THISV.common.string, args[1].common.string);
		VM_PushBool(thread, result);
	}
}
AVES_API NATIVE_FUNCTION(aves_String_contains)
{
	if (!IsString(args[1]))
		VM_ThrowTypeError(thread);

	bool result = String_Contains(THISV.common.string, args[1].common.string);
	VM_PushBool(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_String_startsWith)
{
	if (!IsString(args[1]))
		VM_ThrowTypeError(thread);

	bool result = String_SubstringEquals(THISV.common.string, 0, args[1].common.string);
	VM_PushBool(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_String_endsWith)
{
	if (!IsString(args[1]))
		VM_ThrowTypeError(thread);

	String *other = args[1].common.string;
	bool result = String_SubstringEquals(THISV.common.string,
		THISV.common.string->length - other->length, other);
	VM_PushBool(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_String_reverse)
{
	String *outputString = GC_ConstructString(thread, THISV.common.string->length, nullptr);

	Value *output = VM_Local(thread, 0);
	SetString(output, outputString);

	const uchar *srcp = &THISV.common.string->firstChar;
	uchar *dstp = const_cast<uchar*>(&outputString->firstChar + outputString->length - 1);

	int32_t remaining = outputString->length;
	while (remaining-- > 0)
	{
		if (UC_IsSurrogateLead(*srcp) && UC_IsSurrogateTrail(*(srcp + 1)))
		{
			*reinterpret_cast<uint32_t*>(--dstp) = *reinterpret_cast<const uint32_t*>(srcp++);
			remaining--;
		}
		else
			*dstp = *srcp;
		dstp--;
		srcp++;
	}

	VM_Push(thread, *output);
}
AVES_API NATIVE_FUNCTION(aves_String_substr1)
{
	// substr(start)
	String *str = THISV.common.string;

	int32_t start = GetIndex(thread, str, args + 1);
	int32_t count = str->length - start;

	if (start == 0)
	{
		VM_PushString(thread, str);
		return;
	}
	if (count == 0)
	{
		VM_PushString(thread, strings::Empty);
		return;
	}

	String *outputString = GC_ConstructString(thread, count, &str->firstChar + start);

	Value *output = VM_Local(thread, 0);
	SetString(output, outputString);

	VM_Push(thread, *output);
}
AVES_API NATIVE_FUNCTION(aves_String_substr2)
{
	// substr(start, count)
	String *str = THISV.common.string;

	int32_t start = GetIndex(thread, str, args + 1);
	IntFromValue(thread, args + 2);
	int64_t count = args[2].integer;
	if (start + count > str->length)
	{
		GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}

	if (start == 0 && count == str->length)
	{
		VM_PushString(thread, str);
		return;
	}
	if (count == 0)
	{
		VM_PushString(thread, strings::Empty);
		return;
	}

	String *outputString = GC_ConstructString(thread, (int32_t)count, &str->firstChar + start);

	Value *output = VM_Local(thread, 0);
	SetString(output, outputString);

	VM_Push(thread, *output);
}
AVES_API NATIVE_FUNCTION(aves_String_format)
{
	Value *values = args + 1;

	String *result = nullptr;
	if (IsType(*values, GetType_List()))
		result = string::Format(thread, THISV.common.string, values->common.list);
	else if (IsType(*values, GetType_Hash()))
		result = string::Format(thread, THISV.common.string, values);
	else
		VM_ThrowTypeError(thread, error_strings::FormatValueType);

	VM_PushString(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_String_replaceInner)
{
	// replaceInner(oldValue is String, newValue is String, maxTimes is Int)
	// (Public-facing methods ensure the types are correct)

	String *oldValue = args[1].common.string;
	if (oldValue->length == 0)
	{
		VM_PushString(thread, error_strings::ReplaceEmptyString);
		VM_PushString(thread, strings::oldValue);
		GC_Construct(thread, Types::ArgumentError, 2, nullptr);
		VM_Throw(thread);
	}

	if (args[3].integer == 0) // No replacements to perform! Return 'this'.
	{
		VM_PushString(thread, THISV.common.string);
		return;
	}

	String *newValue = args[2].common.string;

	String *result;
	if (oldValue->length == 1 && newValue->length == 1)
		result = string::Replace(thread, THISV.common.string, oldValue->firstChar, newValue->firstChar, args[3].integer);
	else
		result = string::Replace(thread, THISV.common.string, oldValue, newValue, args[3].integer);

	VM_PushString(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_String_split)
{
	// arguments: (separator)
	// locals: List output
	String *str = THISV.common.string;
	StringFromValue(thread, args + 1);
	String *sep = args[1].common.string;

	Value *output = VM_Local(thread, 0);
	Value ignore;

	if (sep->length == 0) // Split into separate characters
	{
		// Construct the output list
		VM_PushInt(thread, str->length);
		GC_Construct(thread, GetType_List(), 1, output);

		// And then copy each individual character to the output
		const uchar *chp = &str->firstChar;
		int32_t remaining = str->length;
		while (remaining-- > 0)
		{
			VM_Push(thread, *output);
			VM_PushString(thread, GC_ConstructString(thread, 1, chp++));
			VM_InvokeMember(thread, strings::add, 1, &ignore);
		}
	}
	else
	{
		// Construct the output list
		VM_PushInt(thread, str->length / 2);
		GC_Construct(thread, GetType_List(), 1, output);

		const uchar *chp = &str->firstChar;
		const uchar *chStart = chp;
		int32_t index = 0;
		while (index < str->length)
		{
			if (*chp == sep->firstChar)
			{
				if (String_SubstringEquals(str, index, sep))
				{
					// We mound a fatch! I mean, we found a match!
					// Copy characters from chStart to chp into the output,
					// and be aware that chp is inclusive.
					VM_Push(thread, *output);
					if (chp == chStart)
						VM_PushString(thread, strings::Empty);
					else
						VM_PushString(thread, GC_ConstructString(thread, chp - chStart, chStart));
					VM_InvokeMember(thread, strings::add, 1, &ignore);
					index += sep->length;
					chp += sep->length;
					chStart = chp;
					continue;
				}
			}
			index++;
			chp++;
		}

		// And add the last bit of the string, too
		VM_Push(thread, *output);
		if (chStart == &str->firstChar)
			// No match found, just add the entire string
			VM_PushString(thread, str);
		else if (chp == chStart)
			VM_PushString(thread, strings::Empty);
		else
			VM_PushString(thread, GC_ConstructString(thread, chp - chStart, chStart));
		VM_InvokeMember(thread, strings::add, 1, &ignore);
	}

	VM_Push(thread, *output);
}

AVES_API NATIVE_FUNCTION(aves_String_toUpper)
{
	String *result = String_ToUpper(thread, THISV.common.string);
	VM_PushString(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_String_toLower)
{
	String *result = String_ToLower(thread, THISV.common.string);
	VM_PushString(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_String_getCodepoint)
{
	String *str = THISV.common.string;
	int32_t index = GetIndex(thread, str, args + 1);

	const uchar *chp = &str->firstChar + index;

	uint32_t result;
	if (UC_IsSurrogateLead(chp[0]) && UC_IsSurrogateTrail(chp[1]))
		result = UC_ToWide(chp[0], chp[1]);
	else
		result = *chp;

	VM_PushInt(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_String_getCategory)
{
	String *str = THISV.common.string;
	int32_t index = GetIndex(thread, str, args + 1);

	UnicodeCategory cat = UC_GetCategory(&str->firstChar, (unsigned int)index);

	// The values of native type UnicodeCategory are not the same as
	// the values of the Osprey type, so we need to convert!
	uint32_t catValue;
	switch (cat)
	{
		case UC_LETTER_UPPERCASE:    catValue = 1 <<  0; break;
		case UC_LETTER_LOWERCASE:    catValue = 1 <<  1; break;
		case UC_LETTER_TITLECASE:    catValue = 1 <<  2; break;
		case UC_LETTER_MODIFIER:     catValue = 1 <<  3; break;
		case UC_LETTER_OTHER:        catValue = 1 <<  4; break;
		case UC_MARK_NONSPACING:     catValue = 1 <<  5; break;
		case UC_MARK_SPACING:        catValue = 1 <<  6; break;
		case UC_MARK_ENCLOSING:      catValue = 1 <<  7; break;
		case UC_NUMBER_DECIMAL:      catValue = 1 <<  8; break;
		case UC_NUMBER_LETTER:       catValue = 1 <<  9; break;
		case UC_NUMBER_OTHER:        catValue = 1 << 10; break;
		case UC_PUNCT_CONNECTOR:     catValue = 1 << 11; break;
		case UC_PUNCT_DASH:          catValue = 1 << 12; break;
		case UC_PUNCT_OPEN:          catValue = 1 << 13; break;
		case UC_PUNCT_CLOSE:         catValue = 1 << 14; break;
		case UC_PUNCT_INITIAL:       catValue = 1 << 15; break;
		case UC_PUNCT_FINAL:         catValue = 1 << 16; break;
		case UC_PUNCT_OTHER:         catValue = 1 << 17; break;
		case UC_SYMBOL_MATH:         catValue = 1 << 18; break;
		case UC_SYMBOL_CURRENCY:     catValue = 1 << 19; break;
		case UC_SYMBOL_MODIFIER:     catValue = 1 << 20; break;
		case UC_SYMBOL_OTHER:        catValue = 1 << 21; break;
		case UC_SEPARATOR_SPACE:     catValue = 1 << 22; break;
		case UC_SEPARATOR_LINE:      catValue = 1 << 23; break;
		case UC_SEPARATOR_PARAGRAPH: catValue = 1 << 24; break;
		case UC_CONTROL:             catValue = 1 << 25; break;
		case UC_FORMAT:              catValue = 1 << 26; break;
		case UC_SURROGATE:           catValue = 1 << 27; break;
		case UC_PRIVATE_USE:         catValue = 1 << 28; break;
		case UC_UNASSIGNED:          catValue = 1 << 29; break;
		default: catValue = 0; break;
	}

	Value output;
	output.type = Types::UnicodeCategory;
	output.integer = catValue;
	VM_Push(thread, output);
}
AVES_API NATIVE_FUNCTION(aves_String_isSurrogatePair)
{
	String *str = THISV.common.string;
	int32_t index = GetIndex(thread, str, args + 1);

	VM_PushBool(thread, UC_IsSurrogateLead((&str->firstChar)[index]) &&
		UC_IsSurrogateTrail((&str->firstChar)[index + 1]));
}

AVES_API NATIVE_FUNCTION(aves_String_getHashCode)
{
	int32_t hashCode = String_GetHashCode(THISV.common.string);

	VM_PushInt(thread, hashCode);
}

AVES_API NATIVE_FUNCTION(aves_String_fromCodepoint)
{
	IntFromValue(thread, args);
	int64_t cp64 = args[0].integer;

	if (cp64 < 0 || cp64 > 0x10FFFF)
	{
		VM_PushString(thread, strings::cp);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	String *output;
	if (UC_NeedsSurrogatePair((wuchar)cp64))
	{
		SurrogatePair pair = UC_ToSurrogatePair((wuchar)cp64);
		output = GC_ConstructString(thread, 2, reinterpret_cast<uchar*>(&pair));
	}
	else
		output = GC_ConstructString(thread, 1, reinterpret_cast<uchar*>(&cp64));

	// Return value is on the stack
	VM_PushString(thread, output);
}

AVES_API NATIVE_FUNCTION(aves_String_opEquals)
{
	if (!IsString(args[1]))
		VM_PushBool(thread, false);
	else
	{
		bool result = String_Equals(args[0].common.string, args[1].common.string);
		VM_PushBool(thread, result);
	}
}
AVES_API NATIVE_FUNCTION(aves_String_opCompare)
{
	if (!IsString(args[1]))
		VM_ThrowTypeError(thread);

	int result = String_Compare(args[0].common.string, args[1].common.string);
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_String_opMultiply)
{
	String *str = THISV.common.string;
	IntFromValue(thread, args + 1);
	int64_t times = args[1].integer;
	int64_t length = Int_MultiplyChecked(thread, times, str->length);
	if (length > INT32_MAX)
		VM_ThrowOverflowError(thread);

	StringBuffer buf(thread, (int32_t)length);

	for (int64_t i = 0; i < times; i++)
		buf.Append(thread, str);

	String *result = buf.ToString(thread);
	VM_PushString(thread, result);
}

enum class FormatAlignment : signed char
{
	LEFT   = 0,
	CENTER = 1,
	RIGHT  = 2,
};

inline void AppendAlignedFormatString(ThreadHandle thread, StringBuffer &buf, String *value,
									  FormatAlignment alignment, uint32_t alignmentWidth)
{
	uint32_t valueLength = (uint32_t)value->length;
	switch (alignment)
	{
	case FormatAlignment::LEFT:
		buf.Append(thread, value);

		if (valueLength < alignmentWidth)
			buf.Append(thread, alignmentWidth - valueLength, ' ');
		break;
	case FormatAlignment::CENTER:
		if (valueLength < alignmentWidth)
			buf.Append(thread, (alignmentWidth - valueLength) / 2, ' ');

		buf.Append(thread, value);

		if (valueLength < alignmentWidth)
			buf.Append(thread, (alignmentWidth - valueLength + 1) / 2, ' ');
		break;
	case FormatAlignment::RIGHT:
		if (valueLength < alignmentWidth)
			buf.Append(thread, alignmentWidth - valueLength, ' ');

		buf.Append(thread, value);
		break;
	}
}

uint32_t ScanDecimalNumber(ThreadHandle thread, const uchar * &chp, unsigned int &index)
{
	uint32_t result = 0;

	do
	{
		uint32_t num = *chp - '0';

		uint64_t temp = (uint64_t)result * 10ULL + num;
		if (temp > UINT32_MAX)
			VM_ThrowOverflowError(thread);
		result = (uint32_t)temp;

		chp++;
		index++;
	} while (*chp >= '0' && *chp <= '9');

	return result;
}

String *string::Format(ThreadHandle thread, const String *format, ListInst *list)
{
	StringBuffer buf(thread, format->length);

	const uint32_t length = format->length;
	unsigned int start = 0;
	unsigned int index = 0;

	const uchar *chBase = &format->firstChar;
	const uchar *chp = chBase;

	while (index < length)
	{
		const uchar ch = *chp;

		// In all cases, we need to do this anyway, and this makes it slightly
		// faster to look up the next character after 'ch'.
		chp++;

		switch (ch)
		{
		case '{':
			// {} is not allowed, and { must be followed by at least one digit
			if (*chp == '}' || *chp < '0' || *chp > '9')
			{
				VM_PushNull(thread);
				VM_PushString(thread, strings::format);
				GC_Construct(thread, Types::ArgumentError, 2, nullptr);
				VM_Throw(thread);
			}
			// output everything up to (but not including) the {
			if (start < index)
				buf.Append(thread, index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {idx}
			//   {idx<align}  -- left align
			//   {idx>align}  -- right align
			//   {idx=align}  -- center
			// idx and align are always decimal digits, '0'..'9'
			{
				index++;
				uint32_t placeholderIndex = ScanDecimalNumber(thread, chp, index);
				// chp is now after the last digit in the placeholder index

				uint32_t alignmentWidth = 0;
				FormatAlignment alignment = FormatAlignment::LEFT;
				if (*chp == '<' || *chp == '>' || *chp == '=') // alignment follows here, whee
				{
					index++;
					alignment = *chp == '=' ? FormatAlignment::CENTER :
						*chp == '>' ? FormatAlignment::RIGHT :
						FormatAlignment::LEFT;
					chp++;
					alignmentWidth = ScanDecimalNumber(thread, chp, index);
				}

				if (*chp != '}')
				{
					VM_PushNull(thread);
					VM_PushString(thread, strings::format);
					GC_Construct(thread, Types::ArgumentError, 2, nullptr);
					VM_Throw(thread);
				}
				if (placeholderIndex >= (uint32_t)list->length)
				{
					VM_PushNull(thread);
					VM_PushString(thread, strings::format);
					GC_Construct(thread, Types::ArgumentError, 2, nullptr);
					VM_Throw(thread);
				}

				Value *value = VM_Local(thread, 0);
				*value = list->values[placeholderIndex];
				StringFromValue(thread, value);

				AppendAlignedFormatString(thread, buf, value->common.string, alignment, alignmentWidth);
			}

			start = ++index;
			chp++;

			break;
		case '\\':
			if (*chp == '{' || *chp == '}')
			{
				// I actually can't end a comment with a backslash. wtf, C++?
				// Output everything up to (but not including) the backslash
				if (start < index)
					buf.Append(thread, index - start, chBase + start);

				start = ++index; // whee
				chp++; // skip to {
				// fall through to default (the { or } is interpreted literally)
			}
			// else: fall through to default case (the \ is interpreted literally)
		default:
			index++;
			break;
		}
	}

	// and then we append all the remaining characters
	if (start < index)
		buf.Append(thread, index - start, chBase + start);

	String *output = buf.ToString(thread);
	return output;
}

Value ScanFormatIdentifier(ThreadHandle thread, const size_t bufferSize, uchar buffer[],
	unsigned int &index, const uchar * &chp, uint32_t &outLength)
{
	const uchar *chStart = chp;
	// Identifiers follow the following format:
	//     [\p{L}\p{Nl}_][\p{L}\p{Nl}\p{Nd}\p{Mn}\p{Mc}\p{Pc}\p{Cf}]*
	// Note that '_' is part of Pc, which is why it's not explicitly in the second character class.

	uint32_t length = 0;

	bool surrogate;
	UnicodeCategory cat = UC_GetCategory(chp, 0, surrogate);
	if ((cat & UC_TOP_CATEGORY_MASK) != UC_LETTER &&
		cat != UC_NUMBER_LETTER && *chp != '_')
	{
		VM_PushNull(thread);
		VM_PushString(thread, strings::format);
		GC_Construct(thread, Types::ArgumentError, 2, nullptr);
		VM_Throw(thread);
	}

	while (true)
	{
		if (length + surrogate + 1 < bufferSize)
		{
			buffer[length] = *chp;
			if (surrogate)
				buffer[length + 1] = *(chp + 1);
		}
		chp    += 1 + surrogate;
		index  += 1 + surrogate;
		length += 1 + surrogate;

		if (*chp == '}' || *chp == '<' || *chp == '>' || *chp == '=')
			break; // done

		cat = UC_GetCategory(chp, 0, surrogate);
		if (!((cat & UC_TOP_CATEGORY_MASK) == UC_LETTER ||
			cat == UC_NUMBER_LETTER || cat == UC_NUMBER_DECIMAL ||
			cat == UC_MARK_NONSPACING || cat == UC_MARK_SPACING ||
			cat == UC_PUNCT_CONNECTOR || cat == UC_FORMAT))
		{
			VM_PushNull(thread);
			VM_PushString(thread, strings::format);
			GC_Construct(thread, Types::ArgumentError, 2, nullptr);
			VM_Throw(thread);
		}
	}

	outLength = length;

	if (length < bufferSize) // all of the everything is contained within the buffer
	{
		buffer[length] = 0; // trailing 0, always!
		return NULL_VALUE; // indicates that everything is in the buffffffer
	}

	String *outputString = GC_ConstructString(thread, length, chStart);

	Value output;
	SetString(output, outputString);
	return output;
}

String *string::Format(ThreadHandle thread, const String *format, Value *hash)
{
	StringBuffer buf(thread, format->length);

	const uint32_t length = format->length;
	unsigned int start = 0;
	unsigned int index = 0;

	const uchar *chBase = &format->firstChar;
	const uchar *chp = chBase;

	while (index < length)
	{
		const uchar ch = *chp;

		// In all cases, we need to do this anyway, and this makes it slightly
		// faster to look up the next character after 'ch'.
		chp++;

		switch (ch)
		{
		case '{':
			// {} is not allowed
			if (*chp == '}')
			{
				VM_PushNull(thread);
				VM_PushString(thread, strings::format);
				GC_Construct(thread, Types::ArgumentError, 2, nullptr);
				VM_Throw(thread);
			}
			// output everything up to (but not including) the {
			if (start < index)
				buf.Append(thread, index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {name}
			//   {name<align}
			//   {name>align}
			// name is an Osprey identifier; align consists of decimal digits, '0'..'9'
			{
				index++;
				// Most placeholder names are likely to be very short, certainly shorter
				// than 63 characters. Therefore, we have a buffer of 64 uchars, with
				// space reserved for one \0 at the end, which is filled up when reading
				// characters from the string. If only the buffer was used, then ScanFormatIdentifier
				// returns NULL_VALUE, and so we construct a STRING* from the buffer data.
				// If that method does not return NULL_VALUE, then we don't need to do anything,
				// because the method has already GC allocated a STRING* for us.
				const int BUFFER_SIZE = 64; // Note: BUFFER_SIZE includes space for a trailing \0
				LitString<BUFFER_SIZE - 1> buffer = { 0, 0, StringFlags::STATIC };
				uint32_t bufferLength;
				Value phKey = ScanFormatIdentifier(thread, BUFFER_SIZE, buffer.chars, index, chp, bufferLength);
				if (phKey.type == nullptr) // use the values in the buffer
				{
					*const_cast<int32_t*>(&buffer.length) = bufferLength;
					SetString(phKey, _S(buffer));
				}
				// chp is now after the last character in the placeholder name

				uint32_t alignmentWidth = 0;
				FormatAlignment alignment = FormatAlignment::LEFT;
				if (*chp == '<' || *chp == '>' || *chp == '=') // alignment follows here, whee
				{
					index++;
					alignment = *chp == '=' ? FormatAlignment::CENTER :
						*chp == '>' ? FormatAlignment::RIGHT :
						FormatAlignment::LEFT;
					chp++;
					alignmentWidth = ScanDecimalNumber(thread, chp, index);
				}

				if (*chp != '}')
				{
					VM_PushNull(thread);
					VM_PushString(thread, strings::format);
					GC_Construct(thread, Types::ArgumentError, 2, nullptr);
					VM_Throw(thread);
				}

				Value *value = VM_Local(thread, 0);
				VM_Push(thread, *hash);
				VM_Push(thread, phKey);
				VM_LoadIndexer(thread, 1, value);

				StringFromValue(thread, value);

				AppendAlignedFormatString(thread, buf, value->common.string, alignment, alignmentWidth);
			}

			start = ++index;
			chp++;

			break;
		case '\\':
			if (*chp == '{' || *chp == '}')
			{
				// I actually can't end a comment with a backslash. wtf, C++?
				// Output everything up to (but not including) the backslash
				if (start < index)
					buf.Append(thread, index - start, chBase + start);

				start = ++index; // whee
				chp++; // skip to {
				// fall through to default (the { or } is interpreted literally)
			}
			// else: fall through to default case (the \ is interpreted literally)
		default:
			index++;
			break;
		}
	}

	// and then we append all the remaining characters
	if (start < index)
		buf.Append(thread, index - start, chBase + start);

	String *output = buf.ToString(thread);
	return output;
}

String *string::Replace(ThreadHandle thread, const String *input, const uchar oldChar, const uchar newChar, const int64_t maxTimes)
{
	String *output = GC_ConstructString(thread, input->length, &input->firstChar);

	uchar *outp = const_cast<uchar*>(&output->firstChar);
	int64_t remaining = maxTimes;
	int32_t length = input->length;
	while (length-- && (maxTimes < 0 || remaining))
	{
		if (*outp == oldChar)
		{
			*outp = newChar;
			if (maxTimes > 0)
				remaining--;
		}
		outp++;
	}

	return output;
}

String *string::Replace(ThreadHandle thread, const String *input, String *oldValue, String *newValue, const int64_t maxTimes)
{
	StringBuffer buf(thread, input->length);

	const uchar *inp = &input->firstChar;
	int32_t imax = input->length - oldValue->length;

	int32_t start = 0;
	int32_t lengthCollected = 0;
	int64_t remaining = maxTimes;

	int32_t i = 0;
	while (i < imax)
	{
		if (*inp == oldValue->firstChar &&
			String_SubstringEquals(input, start + lengthCollected, oldValue))
		{
			if (lengthCollected > 0)
				buf.Append(thread, lengthCollected, &input->firstChar + start);

			buf.Append(thread, newValue);
			start = start + lengthCollected + oldValue->length;
			lengthCollected = 0;
			inp += oldValue->length;
			i += oldValue->length;

			if (maxTimes > 0)
			{
				if (remaining == 1) // last one!
				{
					// We need to append the rest of the original string now.
					lengthCollected = imax - start;
					break;
				}
				else
					remaining--;
			}
		}
		else
		{
			lengthCollected++;
			inp++;
			i++;
		}
	}

	if (lengthCollected + oldValue->length > 0)
		buf.Append(thread, lengthCollected + oldValue->length, &input->firstChar + start);

	return buf.ToString(thread);
}