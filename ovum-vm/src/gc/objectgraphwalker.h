#pragma once

#include "../vm.h"
#include "gc.h"
#include "../object/type.h"

// The ObjectGraphWalker, as the name implies, walks the object graph. Given a
// GCObject list (in the form of a single GCObject, which makes up the head of
// a doubly linked list), the ObjectGraphWalker visits each GCObject in the
// list, as well as each object's fields.
//
// To prevent extremely deep recursion, the ObjectGraphWalker only visits one
// level of the object graph. It's up to the visitor to collect the objects to
// be visited in the next pass. Thus you get a breadth-first visitor.
//
// An object's fields can contain one of three kinds of values:
//
// * A plain Value (regular and native fields).
// * A String* value (native fields).
// * A GC-managed arrayof unmanaged data (native fields).
//
// In order to visit the object graph, you must implement the methods of the
// prototype ObjectGraphVisitor. The visitor must manage any state it requires
// while processing the object graph.
//
// The visited GCObject can safely be moved to another GCObject list as soon as
// EnterObject() is called for that object, or any time thereafter. The walker
// simply caches the next GCObject.
//
// A class can safely implement both ObjectGraphVisitor and RootSetVisitor. The
// method names do not overlap.

namespace ovum
{

// NOTE: This class is a prototype class! Do not extend it; merely implement the
// methods detailed below, and use the concrete implementation when you call methods
// ObjectGraphWalker.
class ObjectGraphVisitor
{
public:
	// Enters an object. This is called before the fields of an object are
	// visited. If the method returns false, the object is skipped, hence its
	// fields are not examined, and LeaveObject() is not subsequently called.
	//
	// No more than one object will be entered at any given time. That is,
	// objects are never entered recursively.
	bool EnterObject(GCObject *gco);

	// Leaves the current (last entered) object.
	//
	// When EnterObject() returns false for an object, this method is not
	// called for that object.
	//
	// No more than one object will be entered at any given time. That is,
	// objects are never entered recursively.
	void LeaveObject(GCObject *gco);

	// Visits a field containing a Value.
	//
	// If the value has moved from gen0 to gen1, it is sufficient to change the
	// Value's instance pointer. Do not overwrite the entire Value.
	void VisitFieldValue(Value *value);

	// Visits a field containing a String*.
	//
	// If the string value has moved from gen0 to gen1, you will have to overwrite
	// the pointer in the field, which is why this method takes a double pointer.
	void VisitFieldString(String **str);

	// Visits a field containing a GC-managed array of unmanaged data.
	//
	// The value passed to this method is a pointer to the field value; the field in
	// turn contains a pointer to the base of the array, which is the instance base of
	// the GCObject. In other words, use GCObject::FromInst() to get the GCObject*
	// corresponding to the array.
	//
	// If the array value has moved from gen0 to gen1, you will have to overwrite
	// the pointer in the field, which is why this method takes a double pointer.
	void VisitFieldArray(void **arrayBase);
};

template<class Visitor>
class ObjectGraphWalker
{
public:
	static void VisitObjectList(Visitor &visitor, GCObject *head)
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

	static void VisitObject(Visitor &visitor, GCObject *gco)
	{
		if (visitor.EnterObject(gco))
		{
			VisitFields(visitor, gco);
			visitor.LeaveObject(gco);
		}
	}

private:
	static void VisitFields(Visitor &visitor, GCObject *gco)
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

	static void VisitValueArray(Visitor &visitor, size_t count, Value *values)
	{
		for (size_t i = 0; i < count; i++)
			visitor.VisitFieldValue(values + i);
	}

	static void VisitCustomFields(Visitor &visitor, Type *type, void *instanceBase)
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

	static void VisitNativeFields(Visitor &visitor, Type *type, void *instanceBase)
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

	static int OVUM_CDECL ReferenceVisitorCallback(void *state, size_t count, Value *values)
	{
		Visitor *visitor = reinterpret_cast<Visitor*>(state);
		VisitValueArray(*visitor, count, values);
		RETURN_SUCCESS;
	}
};

} // namespace ovum
