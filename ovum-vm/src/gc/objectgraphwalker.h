#pragma once

#include "../vm.h"

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
// In order to visit the object graph, you must implement an ObjectGraphVisitor,
// which is a pure virtual (abstact) class with a variety of Visit* methods.
// The visitor must manage any state it requires while processing the object
// graph.
//
// A class can safely implement both ObjectGraphVisitor and RootSetVisitor. The
// method names do not overlap.

namespace ovum
{

class ObjectGraphVisitor
{
public:
	// Enters an object. This is called before the fields of an object are
	// visited. If the method returns false, the object is skipped, hence its
	// fields are not examined, and LeaveObject() is not subsequently called.
	//
	// No more than one object will be entered at any given time. That is,
	// objects are never entered recursively.
	virtual bool EnterObject(GCObject *gco) = 0;

	// Leaves the current (last entered) object.
	//
	// When EnterObject() returns false for an object, this method is not
	// called for that object.
	//
	// No more than one object will be entered at any given time. That is,
	// objects are never entered recursively.
	virtual void LeaveObject() = 0;

	// Visits a field containing a Value.
	//
	// If the value has moved from gen0 to gen1, it is sufficient to change the
	// Value's instance pointer. Do not overwrite the entire Value.
	virtual void VisitFieldValue(Value *value) = 0;

	// Visits a field containing a String*.
	//
	// If the string value has moved from gen0 to gen1, you will have to overwrite
	// the pointer in the field, which is why this method takes a double pointer.
	virtual void VisitFieldString(String **str) = 0;

	// Visits a field containing a GC-managed array of unmanaged data.
	//
	// The value passed to this method is a pointer to the field value; the field in
	// turn contains a pointer to the base of the array, which is the instance base of
	// the GCObject. In other words, use GCObject::FromInst() to get the GCObject*
	// corresponding to the array.
	//
	// If the array value has moved from gen0 to gen1, you will have to overwrite
	// the pointer in the field, which is why this method takes a double pointer.
	virtual void VisitFieldArray(void **arrayBase) = 0;
};

class ObjectGraphWalker
{
public:
	static void VisitObjectList(ObjectGraphVisitor &visitor, GCObject *head);

	static void VisitObject(ObjectGraphVisitor &visitor, GCObject *gco);

private:
	static void VisitFields(ObjectGraphVisitor &visitor, GCObject *gco);

	static void VisitValueArray(ObjectGraphVisitor &visitor, uint32_t count, Value *values);

	static void VisitCustomFields(ObjectGraphVisitor &visitor, Type *type, void *instanceBase);

	static void VisitNativeFields(ObjectGraphVisitor &visitor, Type *type, void *instanceBase);

	static int OVUM_CDECL ReferenceVisitorCallback(void *state, unsigned int count, Value *values);
};

} // namespace ovum
