#include "CandyPCH.h"

#include "Runtime/Scene/SkeletalAnimationSystem.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/RHI/RHI.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>

namespace Candy {

	namespace {

		// Must match u_Bones[512] in PBR.hlsl (SkinnedVSMain) and the root CBV
		// size limits.
		constexpr uint32_t kMaxBones = 512;

		struct JointTRS
		{
			glm::vec3 Translation = glm::vec3(0.0f);
			glm::quat Rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			glm::vec3 Scale       = glm::vec3(1.0f);
		};

		JointTRS Decompose(const glm::mat4& m)
		{
			JointTRS trs;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(m, trs.Scale, trs.Rotation, trs.Translation, skew, perspective);
			return trs;
		}

		glm::mat4 Compose(const JointTRS& trs)
		{
			return glm::translate(glm::mat4(1.0f), trs.Translation)
				* glm::toMat4(trs.Rotation)
				* glm::scale(glm::mat4(1.0f), trs.Scale);
		}

		glm::quat ToQuat(const glm::vec4& v) { return glm::quat(v.w, v.x, v.y, v.z); }
		glm::vec4 FromQuat(const glm::quat& q) { return glm::vec4(q.x, q.y, q.z, q.w); }

		// Samples one track at `time`; returns the animated value (quaternion
		// as xyzw for rotations).
		glm::vec4 SampleTrack(const AnimationTrack& track, float time)
		{
			const size_t n = track.Times.size();
			if (n == 0)
				return glm::vec4(0.0f);

			// Defensive: Values must hold one entry per key (3 per key for
			// cubic spline). A malformed/mismatched track falls back to rest
			// pose instead of reading out of bounds.
			const size_t needed = (track.Interp == AnimInterp::CubicSpline) ? n * 3 : n;
			if (track.Values.size() < needed)
				return glm::vec4(0.0f);

			const glm::vec4 first = (track.Interp == AnimInterp::CubicSpline) ? track.Values[1] : track.Values[0];
			if (n == 1 || time <= track.Times.front())
				return first;

			const size_t last = n - 1;
			if (time >= track.Times[last])
				return (track.Interp == AnimInterp::CubicSpline) ? track.Values[last * 3 + 1] : track.Values[last];

			const size_t k = std::upper_bound(track.Times.begin(), track.Times.end(), time) - track.Times.begin() - 1;
			const float t0 = track.Times[k];
			const float t1 = track.Times[k + 1];
			const float dt = std::max(t1 - t0, 1e-6f);
			const float u = (time - t0) / dt;

			switch (track.Interp)
			{
				case AnimInterp::Step:
					return track.Values[k];

				case AnimInterp::Linear:
				{
					const glm::vec4 a = track.Values[k];
					const glm::vec4 b = track.Values[k + 1];
					if (track.Path == AnimPath::Rotation)
						return FromQuat(glm::slerp(ToQuat(a), ToQuat(b), u));
					return glm::mix(a, b, u);
				}

				case AnimInterp::CubicSpline:
				{
					// Values layout: (inTangent, value, outTangent) per key.
					const glm::vec4& p0 = track.Values[k * 3 + 1];
					const glm::vec4& p1 = track.Values[(k + 1) * 3 + 1];
					const glm::vec4  m0 = track.Values[k * 3 + 2] * dt;     // out tangent of key k
					const glm::vec4  m1 = track.Values[(k + 1) * 3] * dt;   // in tangent of key k+1
					const float u2 = u * u;
					const float u3 = u2 * u;
					glm::vec4 result =
						(2.0f * u3 - 3.0f * u2 + 1.0f) * p0 + (u3 - 2.0f * u2 + u) * m0
						+ (-2.0f * u3 + 3.0f * u2) * p1 + (u3 - u2) * m1;
					if (track.Path == AnimPath::Rotation)
						result = glm::normalize(result);
					return result;
				}
			}
			return glm::vec4(0.0f);
		}

		const AnimationClip* FindClip(const SkeletalMeshResource& mesh, const std::string& name)
		{
			if (mesh.Clips.empty())
				return nullptr;
			if (!name.empty())
			{
				for (const auto& clip : mesh.Clips)
				{
					if (clip.Name == name)
						return &clip;
				}
			}
			return &mesh.Clips[0];
		}

		// Evaluates the component's pose: `globalPose[j]` = accumulated
		// transform of joint j relative to mesh space (bind-space root).
		void EvaluatePose(const SkeletalMeshComponent& comp, std::vector<glm::mat4>& globalPose)
		{
			const auto& mesh = comp.Mesh;
			const size_t jointCount = mesh->Skeleton.size();
			if (globalPose.size() != jointCount)
				globalPose.assign(jointCount, glm::mat4(1.0f));

			// Default pose = bind-pose rest transforms, decomposed so single
			// channels can be replaced below.
			std::vector<JointTRS> pose(jointCount);
			for (size_t j = 0; j < jointCount; j++)
				pose[j] = Decompose(mesh->Skeleton[j].LocalRestPose);

			const AnimationClip* clip = FindClip(*mesh, comp.ClipName);
			if (clip && !clip->Tracks.empty())
			{
				for (const auto& track : clip->Tracks)
				{
					if (track.JointIndex >= jointCount)
						continue;
					const glm::vec4 v = SampleTrack(track, comp.Time);
					JointTRS& j = pose[track.JointIndex];
					switch (track.Path)
					{
						case AnimPath::Translation: j.Translation = glm::vec3(v); break;
						case AnimPath::Rotation:    j.Rotation = ToQuat(v); break;
						case AnimPath::Scale:       j.Scale = glm::vec3(v); break;
					}
				}
			}

			// Accumulate global poses (topological order: parents come first).
			for (size_t j = 0; j < jointCount; j++)
			{
				const glm::mat4 local = Compose(pose[j]);
				const int32_t parent = mesh->Skeleton[j].ParentIndex;
				globalPose[j] = (parent >= 0) ? globalPose[parent] * local : local;
			}
		}

	} // namespace

	std::unordered_map<const SkeletalMeshResource*, SkeletalAnimationSystem::SkeletonGPU>
		SkeletalAnimationSystem::s_Skeletons;

	void SkeletalAnimationSystem::Update(Scene& scene, float dt)
	{
		auto* dev = RHIContext::GetDevice();
		if (!dev)
			return;

		auto view = scene.GetAllEntitiesWith<SkeletalMeshComponent>();
		for (auto e : view)
		{
			auto& comp = view.get<SkeletalMeshComponent>(e);
			if (!comp.Mesh || comp.Mesh->Skeleton.empty())
				continue;

			// Advance time.
			const AnimationClip* clip = FindClip(*comp.Mesh, comp.ClipName);
			if (comp.Play && dt > 0.0f)
			{
				comp.Time += dt * comp.Speed;
				if (clip && clip->Duration > 0.0f)
				{
					if (comp.Loop)
						comp.Time = std::fmod(comp.Time, clip->Duration);
					else if (comp.Time >= clip->Duration)
					{
						comp.Time = clip->Duration;
						comp.Play = false; // one-shot clip finished
					}
				}
			}

			// Evaluate pose and cache global transforms for skeleton debug lines.
			std::vector<glm::mat4> globalPose;
			EvaluatePose(comp, globalPose);
			comp.DebugGlobalPose = globalPose;

			// Upload skin matrices (global pose * inverse bind) into the bone CB.
			const auto& skeleton = comp.Mesh->Skeleton;
			const uint32_t boneCount = std::min<uint32_t>(
				static_cast<uint32_t>(skeleton.size()), kMaxBones);
			if (boneCount == 0)
				continue;

			auto& gpu = s_Skeletons[comp.Mesh.get()];
			if (!gpu.Buffer || gpu.Joints != boneCount)
			{
				const uint64_t cbSize = ((static_cast<uint64_t>(boneCount) * sizeof(glm::mat4)) + 255u) & ~255ull;
				BufferDesc cb;
				cb.Size          = cbSize;
				cb.Usage         = ResourceUsage::ConstantBuffer;
				cb.CPUAccessible = true;
				cb.DebugName     = "SkeletalAnimation_BoneCB";
				gpu.Buffer = dev->CreateBuffer(cb);
				gpu.Joints = boneCount;
				if (!gpu.Buffer)
					continue;
			}

			std::vector<glm::mat4> skinMatrices(boneCount);
			for (uint32_t j = 0; j < boneCount; j++)
				skinMatrices[j] = globalPose[j] * skeleton[j].InverseBindMatrix;
			gpu.Buffer->Write(skinMatrices.data(), skinMatrices.size() * sizeof(glm::mat4));
		}
	}

	Ref<RHIBuffer> SkeletalAnimationSystem::GetBoneBuffer(const Ref<SkeletalMeshResource>& mesh)
	{
		auto it = s_Skeletons.find(mesh.get());
		if (it == s_Skeletons.end())
			return nullptr;
		return it->second.Buffer;
	}

	void SkeletalAnimationSystem::Shutdown()
	{
		s_Skeletons.clear();
	}

} // namespace Candy
