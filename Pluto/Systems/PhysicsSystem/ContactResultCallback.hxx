#include <btBulletDynamicsCommon.h>

/*
A struct to keep track of contact information.  Can vary depending on what needs to be tracked
*/
struct CollisionContext {
	bool hit;
	const btCollisionObject* body;
	const btCollisionObject* lastBody;
	// GameObject* theObject;
	float distance;
	float velNorm;
	btVector3 point;
	btVector3 normal;
	btVector3 velocity;

	CollisionContext() {
		reset();
	}

	void reset() {
		hit = false;
		body = NULL;
		// theObject = NULL;
		distance = 0.0;
		velNorm = 0.0;
		point.setZero();
		normal.setZero();
		velocity.setZero();
	}
};

/*
A struct to handle contactTest queries via the ContactResultCallback
Based on this example code: http://www.bulletphysics.org/mediawiki-1.5.8/index.php?title=Collision_Callbacks_and_Triggers
*/
struct BulletContactCallback : public btCollisionWorld::ContactResultCallback {
	
	// Constructor, pass whatever context you want to have available when processing contacts
	/* You may also want to set m_collisionFilterGroup and m_collisionFilterMask
	 *  (supplied by the superclass) for needsCollision() */
	BulletContactCallback(btRigidBody& tgtBody , CollisionContext& context /*, ... */)
		: btCollisionWorld::ContactResultCallback(), body(tgtBody), ctxt(context) { }
	
	btRigidBody& body; // The body the sensor is monitoring
	CollisionContext& ctxt; // External information for contact processing
	
	// If you don't want to consider collisions where the bodies are joined by a constraint, override needsCollision:
	/* However, if you use a btCollisionObject for #body instead of a btRigidBody,
	 *  then this is unnecessary -- checkCollideWithOverride isn't available */
	virtual bool needsCollision(btBroadphaseProxy* proxy) const {
		// superclass will check m_collisionFilterGroup and m_collisionFilterMask
		if(!btCollisionWorld::ContactResultCallback::needsCollision(proxy))
			return false;
		// if passed filters, may also want to avoid contacts between constraints
		return body.checkCollideWithOverride(static_cast<btCollisionObject*>(proxy->m_clientObject));
	}
	
	// Called with each contact for your own processing
	virtual btScalar addSingleResult(btManifoldPoint& cp,
		const btCollisionObject* colObj0, int partId0, int index0,
		const btCollisionObject* colObj1, int partId1, int index1) {

		ctxt.hit = true;
		ctxt.lastBody = ctxt.body;
		if(colObj0 == &body) {
			ctxt.point = cp.m_localPointA;
			ctxt.body = colObj1;
		} else {
			assert(colObj1 == &body && "body does not match either collision object");
			ctxt.point = cp.m_localPointB;
			ctxt.body = colObj0;
		}
		// ctxt.theObject = static_cast<GameObject*>(ctxt.body->getUserPointer());
		ctxt.normal = cp.m_normalWorldOnB;
		ctxt.velocity = body.getLinearVelocity();
		ctxt.velNorm = ctxt.normal.dot(ctxt.velocity);

		return 0;
	}
};
