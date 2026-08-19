#include "CandyPCH.h"
#include "Runtime/Asset/SkeletalMeshResource.h"

#include <glm/gtc/constants.hpp>

namespace Candy {

	void SkeletalMeshResource::RecalculateBounds()
	{
		if (Vertices.empty())
		{
			Bounds = {};
			return;
		}

		glm::vec3 minPos = Vertices[0].Position;
		glm::vec3 maxPos = Vertices[0].Position;
		for (const auto& v : Vertices)
		{
			minPos = glm::min(minPos, v.Position);
			maxPos = glm::max(maxPos, v.Position);
		}
		Bounds.Min = minPos;
		Bounds.Max = maxPos;
	}

} // namespace Candy
