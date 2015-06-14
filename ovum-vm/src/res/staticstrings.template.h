#ifndef VM__STATICSTRINGS_H
#define VM__STATICSTRINGS_H

/*********************************************************/
/*                                                       */
/*                 DO NOT EDIT THIS FILE                 */
/*              THIS FILE IS AUTO-GENERATED              */
/*                                                       */
/*  To change this file, edit staticstrings.json and/or  */
/*  staticstrigs.template.h, and run staticstrings.py    */
/*                                                       */
/*********************************************************/

#include "../vm.h"

namespace ovum
{

struct StaticStringData
{
/*@DataMembers@*/
};

class StaticStrings
{
public:
	OVUM_NOINLINE static int Create(StaticStrings *&result);
	~StaticStrings();

/*@StringMembers@*/

private:
	StaticStringData *data;

	StaticStrings();

	void InitData();
	void InitStrings();
};

} // namespace ovum

#endif // VM__STATICSTRINGS_H
