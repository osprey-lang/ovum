#include "objectgraphwalker.h"
#include "gc.h"
#include "../object/type.h"

namespace ovum
{

void ObjectGraphWalker::VisitObjectList(ObjectGraphVisitor &visitor, GCObject *head)
{
	while (head)
	{
		// We have to support situations where the object is removed from
		// the list as part of visiting it, hence we need to cache the next
		// value in the list here. Since we never move backwards, there is
		// no need to cache the previous value.
		GCObject *next = head->next;

		VisitObject(visitor, head);

		head = next;
	}
}

void ObjectGraphWalker::VisitObject(ObjectGraphVisitor &visitor, GCObject *gco)
{
	if (visitor.EnterObject(gco))
	{
		VisitFields(visitor, gco);
		visitor.LeaveObject(gco);
	}
}

void ObjectGraphWalker::VisitFields(ObjectGraphVisitor &visitor, GCObject *gco)
{
	Type *type = gco->type;
	if (reinterpret_cast<uintptr_t>(type) == GC::GC_VALUE_ARRAY)
	{
		size_t length = (gco->size - GCO_SIZE) / sizeof(Value);
		VisitValueArray(visitor, length, gco->FieldsBase());
	}
	else
	{
		while (type)
		{
			if (type->IsCustomPtr())
				VisitCustomFields(visitor, type, gco->InstanceBase(type));
			else if (type->fieldCount)
				VisitValueArray(visitor, type->fieldCount, gco->FieldsBase(type));

			type = type->baseType;
		}
	}
}

void ObjectGraphWalker::VisitValueArray(ObjectGraphVisitor &visitor, size_t count, Value *values)
{
	for (size_t i = 0; i < count; i++)
		visitor.VisitFieldValue(values + i);
}

void ObjectGraphWalker::VisitCustomFields(ObjectGraphVisitor &visitor, Type *type, void *instanceBase)
{
	// Visit native fields first.
	if (type->fieldCount != 0)
		VisitNativeFields(visitor, type, instanceBase);

	// If the type has a reference walker, let's call it.
	if (type->walkReferences)
		type->walkReferences(
			instanceBase,
			ReferenceVisitorCallback,
			&visitor
		);
}

void ObjectGraphWalker::VisitNativeFields(ObjectGraphVisitor &visitor, Type *type, void *instanceBase)
{
	for (size_t i = 0; i < type->fieldCount; i++)
	{
		Type::NativeField field = type->nativeFields[i];
		void *fieldPtr = (char*)instanceBase + field.offset;

		switch (field.type)
		{
		case NativeFieldType::VALUE:
			// The value contained in the field is of type Value.
			visitor.VisitFieldValue(reinterpret_cast<Value*>(fieldPtr));
			break;
		case NativeFieldType::VALUE_PTR:
			// The value contained in the field is of type Value*.
			{
				Value *fieldValue = *reinterpret_cast<Value**>(fieldPtr);
				if (fieldValue != nullptr)
					visitor.VisitFieldValue(*reinterpret_cast<Value**>(fieldPtr));
			}
			break;
		case NativeFieldType::STRING:
			// The value contained in the field is of type String*.
			{
				String *fieldValue = *reinterpret_cast<String**>(fieldPtr);
				if (fieldValue != nullptr)
					visitor.VisitFieldString(reinterpret_cast<String**>(fieldPtr));
			}
			break;
		case NativeFieldType::GC_ARRAY:
			// The GC_ARRAY field contains a pointer to void, which is the instance
			// base of a GCObject. Since fieldPtr is void*, we can't dereference it
			// to get a void*, so we cast it to void** first and dereference that.
			// Remember that fieldPtr is a pointer to the field value, and not the
			// field value itself.
			// I know, it's a bit confusing.
			{
				void *fieldValue = *reinterpret_cast<void**>(fieldPtr);
				if (fieldValue != nullptr)
					visitor.VisitFieldArray(reinterpret_cast<void**>(fieldPtr));
			}
			break;
		default:
			OVUM_UNREACHABLE();
		}
	}
}

int OVUM_CDECL ObjectGraphWalker::ReferenceVisitorCallback(void *state, size_t count, Value *values)
{
	ObjectGraphVisitor *visitor = reinterpret_cast<ObjectGraphVisitor*>(state);
	VisitValueArray(*visitor, count, values);
	RETURN_SUCCESS;
}

} // namespace ovum
