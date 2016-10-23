#include "staticstrings.h"

/*********************************************************/
/*                                                       */
/*                 DO NOT EDIT THIS FILE                 */
/*              THIS FILE IS AUTO-GENERATED              */
/*                                                       */
/*  To change this file, edit staticstrings.json and/or  */
/*  staticstrigs.template.cpp, and run staticstrings.py  */
/*                                                       */
/*********************************************************/

namespace ovum
{

const StaticStringData staticStringData = {
/*@StringData@*/
};

Box<StaticStrings> StaticStrings::New()
{
	Box<StaticStrings> output(new(std::nothrow) StaticStrings());
	if (!output)
		return nullptr;

	if (!output->Init())
		return nullptr;

	return output;
}

StaticStrings::StaticStrings() :
	data(nullptr)
{ }

StaticStrings::~StaticStrings()
{
	free(data);
}

bool StaticStrings::Init()
{
	// StaticStringData has no default constructor, because LitString
	// has const members, so we can't use 'new' here. Since we're going
	// to copy all the string data from a static instance anyway, it's
	// probably fine to use malloc/free for this member.
	data = reinterpret_cast<StaticStringData*>(malloc(sizeof(StaticStringData)));
	if (data == nullptr)
		return false;

	InitData();
	InitStrings();
	return true;
}

void StaticStrings::InitData()
{
	memcpy(this->data, &ovum::staticStringData, sizeof(StaticStringData));
}

void StaticStrings::InitStrings()
{
	StaticStringData *data = this->data;
/*@StringInitializers@*/
}

} // namespace ovum
