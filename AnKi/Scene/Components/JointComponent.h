// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Scene/Components/SceneComponent.h>
#include <AnKi/Physics/PhysicsJoint.h>

namespace anki {

/// @addtogroup scene
/// @{

/// Contains a list of joints.
class JointComponent : public SceneComponent
{
	ANKI_SCENE_COMPONENT(JointComponent)

public:
	JointComponent(SceneNode* node)
		: SceneComponent(node, kClassType)
		, m_node(node)
	{
	}

	~JointComponent();

	/// Create a point 2 point joint on the BodyComponent of the SceneNode.
	void newPoint2PointJoint(const Vec3& relPosFactor, F32 brakingImpulse = kMaxF32);

	/// Create a point 2 point joint on the BodyComponents of the SceneNode and its child node.
	void newPoint2PointJoint2(const Vec3& relPosFactorA, const Vec3& relPosFactorB, F32 brakingImpulse = kMaxF32);

	/// Create a hinge joint on the BodyComponent of the SceneNode.
	void newHingeJoint(const Vec3& relPosFactor, const Vec3& axis, F32 brakingImpulse = kMaxF32);

private:
	class JointNode;

	enum class JointType : U8
	{
		kPoint,
		kHinge,
		kCount
	};

	SceneNode* m_node = nullptr;
	BodyComponent* m_bodyc = nullptr;

	U32 m_parentUuid = 0;

	IntrusiveList<JointNode> m_jointList;

	/// Given a 3 coodrinates that lie in [-1.0, +1.0] compute a pivot point that lies into the AABB of the collision
	/// shape of the body.
	static Vec3 computeLocalPivotFromFactors(const PhysicsBodyPtr& body, const Vec3& factors);

	Error update(SceneComponentUpdateInfo& info, Bool& updated) override;

	void onOtherComponentRemovedOrAdded(SceneComponent* other, Bool added) override;
};
/// @}

} // end namespace anki
