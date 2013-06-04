#include "ov_unicode.internal.h"

// GetMidpoint() returns a weighted/unbalanced/skewed/whatever index,
// closer to imin than imax, because lower Unicode characters are significantly
// more common than the really high ones.
inline int GetMidpoint(int imin, int imax)
{
	return imin + ((imax - imin) >> 2);
}

OVUM_API UnicodeCategory UC_GetCategory(const uchar ch)
{
	const UnicodeRange *ranges = reinterpret_cast<const UnicodeRange*>(Ranges);

	int imin = 0, imax = UNI_RANGE_COUNT - 1;
	while (imin <= imax)
	{
		int i = GetMidpoint(imin, imax);

		assert(i >= 0 && i < UNI_RANGE_COUNT);

		UnicodeRange r = ranges[i];
		if (ch < r.start)
		{
			// If i == 0, then we will never even reach this point: ranges[0].start == 0,
			// and uchar is unsigned (hence ch < r.start is false).
			// Therefore, there's no need to test whether i > 0 before subtracting 1.
			//
			// If ranges[0] changes so that its .start is no longer 0, the below code
			// will almost certainly blow up. Or not. Who knows. It's C++.
			if (ch > ranges[i - 1].end)
				// The character is between the current range and the previous.
				return CharCategories[ch - ranges[i - 1].offset];
			else // ch is either in or before the previous range; update the bounds!
				imax = i - 1;
		}
		else if (ch > r.end)
		{
			// If there is no next range (i.e. i == UNI_RANGE_COUNT - 1), then the character
			// must be past the end of the ranges.
			// Note: r.offset is for characters between r.end and ranges[i + 1].start.
			if (i == UNI_RANGE_COUNT - 1 || ch < ranges[i + 1].start)
				// The character is between the current range and the next.
				return CharCategories[ch - r.offset];
			else // ch is either in or after the next range.
				imin = i + 1;
		}
		else
			return static_cast<UnicodeCategory>(r.category);
	}
	// This point should never be reached. One of the paths above is guaranteed to return.
	// However, the VC++ compiler can't figure this out, so we need this return statement.
#ifndef NDEBUG
	throw L"UC_GetCategory fell through the loop!";
#else
	return UC_UNASSIGNED;
#endif
}

OVUM_API UnicodeCategory UC_GetCategoryW(const wuchar ch)
{
	assert_valid_wuchar(ch);
	const WUnicodeRange *ranges = reinterpret_cast<const WUnicodeRange*>(WRanges);

	// See GetCategory(const uchar) for documentation.
	// This is basically the same but with wide stuff.

	// NOTE: WCharCategories begins at U+10000! We have to
	// subtract 0x10000 from the index whenever accessing
	// a value in ranges[].

	int imin = 0, imax = UNI_WRANGE_COUNT - 1;
	while (imin <= imax)
	{
		int i = GetMidpoint(imin, imax);

		assert(i >= 0 && i < UNI_WRANGE_COUNT);

		WUnicodeRange r = ranges[i];
		if (ch < r.start)
		{
			if (ch > ranges[i - 1].end)
				return WCharCategories[ch - 0x10000 - ranges[i - 1].offset];
			else
				imax = i - 1;
		}
		else if (ch > r.end)
		{
			if (i == UNI_WRANGE_COUNT - 1 || ch < ranges[i + 1].start)
				return WCharCategories[ch - 0x10000 - r.offset];
			else
				imin = i + 1;
		}
		else
			return static_cast<UnicodeCategory>(r.category);
	}
#ifndef NDEBUG
	throw L"UC_GetCategoryW fell through the loop!";
#else
	return UC_UNASSIGNED;
#endif
}

OVUM_API CaseMap UC_GetCaseMap(const uchar ch)
{
	const CaseMap *cases = reinterpret_cast<const CaseMap*>(CaseMaps);

	// This is a simple binary search implementation, with a slight change:
	// if ch is between the current and the next CASEMAP, we break.
	// Instead of throwing if the value is not found, we just return ch, since
	// the character obviously doesn't have any casemap associated with it.

	int imin = 0, imax = UNI_CASEMAP_COUNT - 1;
	while (imin <= imax)
	{
		int i = GetMidpoint(imin, imax);

		assert(i >= 0 && i < UNI_CASEMAP_COUNT);

		CaseMap cs = cases[i];
		if (ch < cs.codepoint)
		{
			if (i == 0 || ch > cases[i - 1].codepoint)
				break;
			imax = i - 1;
		}
		else if (ch > cs.codepoint)
		{
			if (i == UNI_CASEMAP_COUNT - 1 || ch < cases[i + 1].codepoint)
				break;
			imin = i + 1;
		}
		else
			return cs;
	}

	CaseMap dflt = { ch, ch, ch };
	return dflt;
}

OVUM_API WCaseMap UC_GetCaseMapW(const wuchar ch)
{
	assert_valid_wuchar(ch);
	const WCaseMap *cases = reinterpret_cast<const WCaseMap*>(WCaseMaps);

	int imin = 0, imax = UNI_WCASEMAP_COUNT - 1;
	while (imin <= imax)
	{
		int i = GetMidpoint(imin, imax);

		assert(i >= 0 && i < UNI_WCASEMAP_COUNT);

		WCaseMap cs = cases[i];
		if (ch < cs.codepoint)
		{
			if (i == 0 || ch > cases[i - 1].codepoint)
				break;
			imax = i - 1;
		}
		else if (ch > cs.codepoint)
		{
			if (i == UNI_WCASEMAP_COUNT - 1 || ch < cases[i + 1].codepoint)
				break;
			imin = i + 1;
		}
		else
			return cs;
	}

	WCaseMap dflt = { ch, ch, ch };
	return dflt;
}