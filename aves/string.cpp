#include "aves_string.h"
#include "ov_string.h"
#include "ov_stringbuffer.h"
#include "ov_unicode.h"

AVES_API NATIVE_FUNCTION(aves_String_get_item)
{
	int64_t index = IntFromValue(thread, args[1]).integer;

	String *str = THISV.common.string;
	if (index < 0 || index >= str->length)
	{
		GC_Construct(thread, ArgumentRangeError, 0, NULL);
		VM_Throw(thread);
	}

	String *output;
	GC_ConstructString(thread, 1, &str->firstChar + (int32_t)index, &output);
	VM_PushString(thread, output);
}

AVES_API NATIVE_FUNCTION(aves_String_get_length)
{
	VM_PushInt(thread, THISV.common.string->length);
}

AVES_API NATIVE_FUNCTION(aves_String_getCategory)
{
	int64_t index = IntFromValue(thread, args[1]).integer;

	String *str = THISV.common.string;
	if (index < 0 || index > str->length)
	{
		GC_Construct(thread, ArgumentRangeError, 0, NULL);
		VM_Throw(thread);
	}

	UnicodeCategory cat = UC_GetCategory(&str->firstChar, (unsigned int)index);

	Value output;
	output.type = UnicodeCategoryType;
	output.integer = (int64_t)cat;
	VM_Push(thread, output);
}
AVES_API NATIVE_FUNCTION(aves_String_format)
{
	Value *values = args + 1;

	String *result = NULL;
	if (IsType(*values, GetType_List()))
		result = string::Format(thread, THISV.common.string, values->common.list);
	else if (IsType(*values, GetType_Hash()))
		result = string::Format(thread, THISV.common.string, values);
	else
		VM_ThrowTypeError(thread);

	VM_PushString(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_String_getHashCode)
{
	int32_t hashCode = String_GetHashCode(THISV.common.string);

	VM_PushInt(thread, hashCode);
}
AVES_API NATIVE_FUNCTION(aves_String_toString)
{
	VM_Push(thread, THISV);
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
				GC_Construct(thread, ArgumentError, 1, NULL);
				VM_Throw(thread);
			}
			// output everything up to (but not including) the {
			if (start < index)
				buf.Append(thread, index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {idx}
			//   {idx,align}
			//   {idx,-align}
			// idx and align are always decimal digits, '0'..'9'
			{
				index++;
				uint32_t placeholderIndex = ScanDecimalNumber(thread, chp, index);
				// chp is now after the last digit in the placeholder index

				uint32_t alignment = 0;
				bool alignRight = false;
				if (*chp == ',') // alignment follows here, whee
				{
					index++;
					chp++;
					if (*chp == '-') // align right
					{
						alignRight = true;
						index++;
						chp++;
					}
					alignment = ScanDecimalNumber(thread, chp, index);
				}

				if (*chp != '}')
				{
					VM_PushString(thread, strings::format);
					GC_Construct(thread, ArgumentError, 1, NULL);
					VM_Throw(thread);
				}
				if (placeholderIndex >= list->length)
				{
					VM_PushString(thread, strings::format);
					GC_Construct(thread, ArgumentError, 1, NULL);
					VM_Throw(thread);
				}

				Value value = StringFromValue(thread, list->values[placeholderIndex]);

				if (alignRight && value.common.string->length < alignment)
					buf.Append(thread, alignment - value.common.string->length, ' ');

				buf.Append(thread, value.common.string);

				if (!alignRight && value.common.string->length < alignment)
					buf.Append(thread, alignment - value.common.string->length, ' ');
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
	unsigned int &index, const uchar * &ch, uint32_t &outLength)
{
	const uchar *chStart = ch;
	// Identifiers follow the following format:
	//     [\p{L}\p{Nl}_][\p{L}\p{Nl}\p{Nd}\p{Mn}\p{Mc}\p{Pc}\p{Cf}]*
	// Note that '_' is part of Pc, which is why it's not explicitly in the second character class.

	uint32_t length = 0;

	bool surrogate;
	UnicodeCategory cat = UC_GetCategory(ch, 0, surrogate);
	if ((cat & UC_TOP_CATEGORY_MASK) != UC_LETTER &&
		cat != UC_NUMBER_LETTER && *ch != '_')
	{
		VM_PushString(thread, strings::format);
		GC_Construct(thread, ArgumentError, 1, NULL);
		VM_Throw(thread);
	}

	while (true)
	{
		if (length + surrogate + 1 < bufferSize)
		{
			buffer[length] = *ch;
			if (surrogate)
				buffer[length + 1] = *(ch + 1);
		}
		ch     += 1 + surrogate;
		index  += 1 + surrogate;
		length += 1 + surrogate;

		if (*ch == '}' || *ch == ',')
			break; // done

		cat = UC_GetCategory(ch, 0, surrogate);
		if (!((cat & UC_TOP_CATEGORY_MASK) == UC_LETTER ||
			cat == UC_NUMBER_LETTER || cat == UC_NUMBER_DECIMAL ||
			cat == UC_MARK_NONSPACING || cat == UC_MARK_SPACING ||
			cat == UC_PUNCT_CONNECTOR || cat == UC_FORMAT))
		{
			VM_PushString(thread, strings::format);
			GC_Construct(thread, ArgumentError, 1, NULL);
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
				GC_Construct(thread, ArgumentError, 1, NULL);
				VM_Throw(thread);
			}
			// output everything up to (but not including) the {
			if (start < index)
				buf.Append(thread, index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {name}
			//   {name,align}
			//   {name,-align}
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
				uchar chbuffer[BUFFER_SIZE];
				uint32_t bufferLength;
				Value phKey = ScanFormatIdentifier(thread, BUFFER_SIZE, chbuffer, index, chp, bufferLength);
				if (phKey.type == NULL) // use the values in the buffer
				{
					// Note: BUFFER_SIZE includes space for a trailing \0
					LitString<BUFFER_SIZE - 1> keyString = { bufferLength, 0, STR_STATIC };
					CopyMemoryT(keyString.chars, chbuffer, bufferLength);
					SetString(phKey, reinterpret_cast<String*>(&keyString));
				}
				// chp is now after the last digit in the placeholder name

				uint32_t alignment = 0;
				bool alignRight = false;
				if (*chp == ',') // alignment follows here, whee
				{
					index++;
					chp++;
					if (*chp == '-') // align right
					{
						alignRight = true;
						index++;
						chp++;
					}
					alignment = ScanDecimalNumber(thread, chp, index);
				}

				if (*chp != '}')
				{
					VM_PushString(thread, strings::format);
					GC_Construct(thread, ArgumentError, 1, NULL);
					VM_Throw(thread);
				}

				Value value;
				VM_Push(thread, *hash);
				VM_Push(thread, phKey);
				VM_LoadIndexer(thread, 1, &value);

				value = StringFromValue(thread, value);

				if (alignRight && value.common.string->length < alignment)
					buf.Append(thread, alignment - value.common.string->length, ' ');

				buf.Append(thread, value.common.string);

				if (!alignRight && value.common.string->length < alignment)
					buf.Append(thread, alignment - value.common.string->length, ' ');
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