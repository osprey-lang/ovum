#pragma once

#ifndef VM__UNICODE_INTERNAL_H
#define VM__UNICODE_INTERNAL_H

#include "ov_vm.internal.h"
#include "ov_unicode.h"

typedef struct UnicodeRange_S
{
	const uint16_t category;
	const uchar start, end;
	const uint16_t offset;
} UnicodeRange;
typedef struct WUnicodeRange_S
{
	const uint32_t category;
	const wuchar start, end;
	const uint32_t offset;
} WUnicodeRange;

// NB: these #defines are produced by unitables.pl
// If you run unitables.pl, don't forget to update these!

#define UNI_RANGE_COUNT    150
#define UNI_CASEMAP_COUNT  2010
#define UNI_WRANGE_COUNT   128
#define UNI_WCASEMAP_COUNT 80

extern const UnicodeCategory CharCategories[];
extern const uint16_t Ranges[];
extern const uchar CaseMaps[];

extern const UnicodeCategory WCharCategories[];
extern const uint32_t WRanges[];
extern const wuchar WCaseMaps[];

#endif // VM__UNICODE_INTERNAL_H