#include <PxPhysicsAPI.h>
#include <spdlog/spdlog.h>

#include "Physics.hpp"

class UserErrorCallback : public physx::PxErrorCallback
{
public:
	virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		SPDLOG_ERROR("NVIDIA PhysX - \"{}\" at {} : {}", message, file, line);
	}
} g_physxErrorCallback;

namespace Physics
{
physx::PxFoundation* g_foundation = nullptr;
physx::PxPhysics* g_physics = nullptr;
physx::PxDefaultAllocator g_allocator;
physx::PxDefaultCpuDispatcher* g_cpuDispatcher = nullptr;
physx::PxScene* g_scene = nullptr;

// Ground
physx::PxMaterial* g_material = nullptr;
physx::PxRigidStatic* g_ground = nullptr;

// Box
physx::PxTransform g_boxTransform = physx::PxTransform(0, 5, 0);
physx::PxBoxGeometry g_boxGeometry = physx::PxVec3(1, 1, 1);
physx::PxRigidDynamic* g_box = nullptr;

bool Init()
{
	g_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_allocator, g_physxErrorCallback);
	if (!g_foundation) {
		SPDLOG_ERROR("NVIDIA PhysX - PxCreateFoundation error!");
		return false;
	}

	// PvD and Transport later...

	g_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_foundation, physx::PxTolerancesScale(), true /* PvD later... */);
	if (!g_physics) {
		SPDLOG_ERROR("NVIDIA PhysX - PxCreatePhysics error!");
		return false;
	}

	physx::PxSceneDesc sceneDesc(g_physics->getTolerancesScale());
	sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f); // Earth's gravity

	g_cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(2);
	sceneDesc.cpuDispatcher = g_cpuDispatcher;
	sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader; // TODO: Change later?

	g_scene = g_physics->createScene(sceneDesc);
	if (!g_scene) {
		SPDLOG_ERROR("NVIDIA PhysX - createScene error!");
		return false;
	}

	// Static ground plane
	g_material = g_physics->createMaterial(0.5f, 0.5f, 0.6f);
	g_ground = physx::PxCreatePlane(*g_physics, physx::PxPlane(0, 1, 0, 0), *g_material);
	g_scene->addActor(*g_ground);

	// Dynamic box
	g_box = physx::PxCreateDynamic(*g_physics, g_boxTransform, g_boxGeometry, *g_material, 1.0f);
	g_scene->addActor(*g_box);

	return true;
}
void Step()
{
	// Run at 75 FPS
	g_scene->simulate(1.0f / 75.0f);
	g_scene->fetchResults(true);

	physx::PxTransform updatedBoxPos = g_box->getGlobalPose();
	if (updatedBoxPos != g_boxTransform) {
		SPDLOG_INFO("Box = {{{}, {}, {}}}", updatedBoxPos.p.x, updatedBoxPos.p.y, updatedBoxPos.p.z);
		g_boxTransform = updatedBoxPos;
	}
}
void Terminate()
{
	g_box->release();
	g_ground->release();
	g_scene->release();
	g_cpuDispatcher->release();
	g_physics->release();
	g_foundation->release();
}
}