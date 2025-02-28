// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Util/Assert.h>
#include <AnKi/Util/List.h>
#include <AnKi/Util/StdTypes.h>
#include <algorithm>

namespace anki {

/// @addtogroup util_patterns
/// @{

/// A hierarchical object
template<typename T, typename TMemoryPool>
class Hierarchy
{
public:
	using Value = T;
	using Container = List<Value*, TMemoryPool>;

	Hierarchy(const TMemoryPool& pool = TMemoryPool())
		: m_children(pool)
	{
	}

	Hierarchy(const Hierarchy&) = delete; // Non-copyable

	~Hierarchy()
	{
		destroy();
	}

	Hierarchy& operator=(const Hierarchy&) = delete; // Non-copyable

	void destroy();

	const Value* getParent() const
	{
		return m_parent;
	}

	Value* getParent()
	{
		return m_parent;
	}

	Value& getChild(U32 i)
	{
		return *(*(m_children.getBegin() + i));
	}

	const Value& getChild(U32 i) const
	{
		return *(*(m_children.getBegin() + i));
	}

	Bool hasChildren() const
	{
		return !m_children.isEmpty();
	}

	/// Add a new child.
	void addChild(Value* child);

	/// Remove a child.
	void removeChild(Value* child);

	void setParent(Value* parent)
	{
		ANKI_ASSERT(parent);
		parent->addChild(static_cast<Value*>(this));
	}

	void removeParent()
	{
		if(m_parent)
		{
			m_parent->removeChild(this);
		}
	}

	/// Visit the children and the children's children. Use it with lambda
	template<typename TVisitorFunc>
	Bool visitChildren(TVisitorFunc vis);

	/// Visit this object and move to the children. Use it with lambda
	template<typename TVisitorFunc>
	void visitThisAndChildren(TVisitorFunc vis);

	/// Visit the whole tree. Use it with lambda
	template<typename TVisitorFunc>
	void visitTree(TVisitorFunc vis);

	/// Visit the children and limit the depth. Use it with lambda.
	template<typename TVisitorFunc>
	Bool visitChildrenMaxDepth(I maxDepth, TVisitorFunc vis);

private:
	Value* m_parent = nullptr; ///< May be nullptr
	Container m_children;

	/// Cast the Hierarchy to the given type
	Value* getSelf()
	{
		return static_cast<Value*>(this);
	}

	/// Find the child
	typename Container::Iterator findChild(Value* child)
	{
		typename Container::Iterator it = m_children.find(child);
		return it;
	}
};
/// @}

} // end namespace anki

#include <AnKi/Util/Hierarchy.inl.h>
