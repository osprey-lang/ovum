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
		visitor.LeaveObject();
	}
}

void ObjectGraphWalker::VisitFields(ObjectGraphVisitor &visitor, GCObject *gco)
{
	Type *type = gco->type;
	if (type == (Type*)GC::GC_VALUE_ARRAY)
	{
		uint32_t length = static_cast<uint32_t>((gco->size - GCO_SIZE) / sizeof(Value));
		VisitValueArray(visitor, length, gco->FieldsBase());
	}
	else
	{
		while (type)
		{
			if (type->IsCustomPtr())
				VisitCustomFields(visitor, type, gco->InstanceBase(type));
			else if (type->fieldCount)
				VisitValueArray(visitor, static_cast<uint32_t>(type->fieldCount), gco->FieldsBase(type));

			type = type->baseType;
		}
	}
}

void ObjectGraphWalker::VisitValueArray(ObjectGraphVisitor &visitor, uint32_t count, Value *values)
{
	for (uint32_t i = 0; i < count; i++)
		visitor.VisitFieldValue(values + i);
}

void ObjectGraphWalker::VisitCustomFields(ObjectGraphVisitor &visitor, Type *type, void *instanceBase)
{
	// Visit native fields first.
	if (type->fieldCount != 0)
		VisitNativeFields(visitor, type, instanceBase);

	// If the type has a reference visitor, let's call it.
	if (type->getReferences)
		type->getReferences(
			instanceBase,
			ReferenceVisitorCallback,
			&visitor
		);
}

void ObjectGraphWalker::VisitNativeFields(ObjectGraphVisitor &visitor, Type *type, void *instanceBase)
{
	for (int i = 0; i < type->fieldCount; i++)
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
			// The GC_ARRAY field contains a pointer to void*, which is the instance
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
		}
	}
}

int OVUM_CDECL ObjectGraphWalker::ReferenceVisitorCallback(void *state, unsigned int count, Value *values)
{
	ObjectGraphVisitor *visitor = reinterpret_cast<ObjectGraphVisitor*>(state);
	VisitValueArray(*visitor, static_cast<uint32_t>(count), values);
	RETURN_SUCCESS;
}

} // namespace ovum
