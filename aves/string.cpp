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
	int64_t index = IntFromValue(thread, *arg).integer;
	if (index < 0 || index >= str->length)
	{
		VM_PushString(thread, error_strings::IndexOutOfRange);
		GC_Construct(thread, ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	return (int32_t)index;
}

AVES_API NATIVE_FUNCTION(aves_String_get_item)
{
	String *str = THISV.common.string;

	int32_t index = GetIndex(thread, str, args + 1);

	String *output;
	GC_ConstructString(thread, 1, &str->firstChar + index, &output);
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
	Value other = StringFromValue(thread, args[1]);

	bool result = String_Contains(THISV.common.string, other.common.string);
	VM_PushBool(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_String_reverse)
{
	String *outputString;
	GC_ConstructString(thread, THISV.common.string->length, nullptr, &outputString);

	Value *output = VM_Local(thread, 0);
	SetString(output, outputString);

	const uchar *srcp = &THISV.common.string->firstChar;
	uchar *dstp = const_cast<uchar*>(&outputString->firstChar + outputString->length - 1);

	int32_t remaining = outputString->length;
	while (remaining > 0)
	{
		if (UC_IsSurrogateLead(*srcp) && UC_IsSurrogateTrail(*(srcp + 1)))
			*reinterpret_cast<uint32_t*>(--dstp) = *reinterpret_cast<const uint32_t*>(srcp++);
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

	if (count == 0)
	{
		VM_PushString(thread, strings::Empty);
		return;
	}

	String *outputString;
	GC_ConstructString(thread, count, &str->firstChar + start, &outputString);

	Value *output = VM_Local(thread, 0);
	SetString(output, outputString);

	VM_Push(thread, *output);
}
AVES_API NATIVE_FUNCTION(aves_String_substr2)
{
	// substr(start, count)
	String *str = THISV.common.string;

	int32_t start = GetIndex(thread, str, args + 1);
	int64_t count = IntFromValue(thread, args[2]).integer;
	if (start + count >= str->length)
	{
		GC_Construct(thread, ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}

	if (count == 0)
	{
		VM_PushString(thread, strings::Empty);
		return;
	}

	String *outputString;
	GC_ConstructString(thread, (int32_t)count, &str->firstChar + start, &outputString);

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
		GC_Construct(thread, ArgumentError, 2, nullptr);
		VM_Throw(thread);
	}

	String *newValue = args[2].common.string;

	String *result;
	if (oldValue->length == 1 && newValue->length == 1)
		result = string::Replace(thread, THISV.common.string, oldValue->firstChar, newValue->firstChar, args[3].integer);
	else
		result = string::Replace(thread, THISV.common.string, oldValue, newValue, args[3].integer);

	VM_PushString(thread, result);
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

AVES_API NATIVE_FUNCTION(aves_String_getCategory)
{
	String *str = THISV.common.string;
	int32_t index = GetIndex(thread, str, args + 1);

	UnicodeCategory cat = UC_GetCategory(&str->firstChar, (unsigned int)index);

	Value output;
	output.type = UnicodeCategoryType;
	output.integer = (int64_t)cat;
	VM_Push(thread, output);
}

AVES_API NATIVE_FUNCTION(aves_String_getHashCode)
{
	int32_t hashCode = String_GetHashCode(THISV.common.string);

	VM_PushInt(thread, hashCode);
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
	int64_t times = IntFromValue(thread, args[1]).integer;
	int64_t length = Int_MultiplyChecked(thread, times, str->length);
	if (length > INT32_MAX)
		VM_ThrowOverflowError(thread);

	StringBuffer buf(thread, (int32_t)length);

	for (int64_t i = 0; i < times; i++)
		buf.Append(thread, str);

	String *result = buf.ToString(thread);
	VM_PushString(thread, result);
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
				VM_PushString(thread, strings::format);
				GC_Construct(thread, ArgumentError, 1, nullptr);
				VM_Throw(thread);
			}
			// output everything up to (but not including) the {
			if (start < index)
				buf.Append(thread, index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {idx}
			//   {idx<align}
			//   {idx>align}
			// idx and align are always decimal digits, '0'..'9'
			{
				index++;
				uint32_t placeholderIndex = ScanDecimalNumber(thread, chp, index);
				// chp is now after the last digit in the placeholder index

				uint32_t alignment = 0;
				bool alignRight = false;
				if (*chp == '<' || *chp == '>') // alignment follows here, whee
				{
					index++;
					alignRight = *chp == '>';
					chp++;
					alignment = ScanDecimalNumber(thread, chp, index);
				}

				if (*chp != '}')
				{
					VM_PushString(thread, strings::format);
					GC_Construct(thread, ArgumentError, 1, nullptr);
					VM_Throw(thread);
				}
				if (placeholderIndex >= list->length)
				{
					VM_PushString(thread, strings::format);
					GC_Construct(thread, ArgumentError, 1, nullptr);
					VM_Throw(thread);
				}

				Value *value = VM_Local(thread, 0);
				*value = StringFromValue(thread, list->values[placeholderIndex]);

				if (alignRight && value->common.string->length < alignment)
					buf.Append(thread, alignment - value->common.string->length, ' ');

				buf.Append(thread, value->common.string);

				if (!alignRight && value->common.string->length < alignment)
					buf.Append(thread, alignment - value->common.string->length, ' ');
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
		VM_PushString(thread, strings::format);
		GC_Construct(thread, ArgumentError, 1, nullptr);
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

		if (*chp == '}' || *chp == ',')
			break; // done

		cat = UC_GetCategory(chp, 0, surrogate);
		if (!((cat & UC_TOP_CATEGORY_MASK) == UC_LETTER ||
			cat == UC_NUMBER_LETTER || cat == UC_NUMBER_DECIMAL ||
			cat == UC_MARK_NONSPACING || cat == UC_MARK_SPACING ||
			cat == UC_PUNCT_CONNECTOR || cat == UC_FORMAT))
		{
			VM_PushString(thread, strings::format);
			GC_Construct(thread, ArgumentError, 1, nullptr);
			VM_Throw(thread);
		}
	}

	outLength = length;

	if (length < bufferSize) // all of the everything is contained within the buffer
	{
		buffer[length] = 0; // trailing 0, always!
		return NULL_VALUE; // indicates that everything is in the buffffffer
	}

	String *outputString;
	GC_ConstructString(thread, length, chStart, &outputString);

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
				VM_PushString(thread, strings::format);
				GC_Construct(thread, ArgumentError, 1, nullptr);
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
				// than 127 characters. Therefore, we have a buffer of 128 uchars, with
				// space reserved for one \0 at the end, which is filled up when reading
				// characters from the string. If only the buffer was used, then ScanFormatIdentifier
				// returns NULL_VALUE, and so we construct a STRING* from the buffer data.
				// If that method does not return NULL_VALUE, then we don't need to do anything,
				// because the method has already GC allocated a STRING* for us.
				const int BUFFER_SIZE = 128; // Note: BUFFER_SIZE includes space for a trailing \0
				LitString<BUFFER_SIZE - 1> buffer = { 0, 0, STR_STATIC };
				uint32_t bufferLength;
				Value phKey = ScanFormatIdentifier(thread, BUFFER_SIZE, buffer.chars, index, chp, bufferLength);
				if (phKey.type == nullptr) // use the values in the buffer
				{
					*const_cast<int32_t*>(&buffer.length) = bufferLength;
					SetString(phKey, _S(buffer));
				}
				// chp is now after the last character in the placeholder name

				uint32_t alignment = 0;
				bool alignRight = false;
				if (*chp == '<' || *chp == '>') // alignment follows here, whee
				{
					index++;
					alignRight = *chp == '>';
					chp++;
					alignment = ScanDecimalNumber(thread, chp, index);
				}

				if (*chp != '}')
				{
					VM_PushString(thread, strings::format);
					GC_Construct(thread, ArgumentError, 1, nullptr);
					VM_Throw(thread);
				}

				Value *value = VM_Local(thread, 0);
				VM_Push(thread, *hash);
				VM_Push(thread, phKey);
				VM_LoadIndexer(thread, 1, value);

				*value = StringFromValue(thread, *value);

				if (alignRight && value->common.string->length < alignment)
					buf.Append(thread, alignment - value->common.string->length, ' ');

				buf.Append(thread, value->common.string);

				if (!alignRight && value->common.string->length < alignment)
					buf.Append(thread, alignment - value->common.string->length, ' ');
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
	String *output;
	GC_ConstructString(thread, input->length, &input->firstChar, &output);

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
	while (i < imax && (maxTimes < 0 || remaining))
	{
		if (*inp == oldValue->firstChar &&
			String_SubstringEquals(input, start + lengthCollected, oldValue))
		{
			if (lengthCollected > 0)
				buf.Append(thread, lengthCollected, &input->firstChar + start);

			buf.Append(thread, newValue);
			start = start + lengthCollected + oldValue->length;
			lengthCollected = 0;
			i += oldValue->length;

			if (maxTimes > 0)
				remaining--;
		}
		else
		{
			lengthCollected++;
			i++;
		}
	}

	if (lengthCollected > 0)
		buf.Append(thread, lengthCollected, &input->firstChar + start);

	return buf.ToString(thread);
}