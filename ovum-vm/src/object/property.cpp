#include "property.h"

namespace ovum
{

// Nothing to put here yet!

} // namespace ovum

OVUM_API MethodHandle Property_GetGetter(PropertyHandle prop)
{
	return prop->getter;
}
OVUM_API MethodHandle Property_GetSetter(PropertyHandle prop)
{
	return prop->setter;
}
