#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Font.hpp>
#include <KlayGE/ElementFormat.hpp>
#include <KlayGE/Framebuffer.hpp>
#include <KlayGE/RenderLayout.hpp>
#include <KlayGE/Renderable.hpp>
#include <KlayGE/RenderableHelper.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/RenderEffect.hpp>
#include <KlayGE/SceneManager.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/ResLoader.hpp>
#include <KlayGE/RenderSettings.hpp>
#include <KlayGE/GraphicsBuffer.hpp>
#include <KlayGE/SceneObjectHelper.hpp>
#include <KlayGE/PostProcess.hpp>
#include <KlayGE/Timer.hpp>
#include <KlayGE/InfTerrain.hpp>
#include <KlayGE/LensFlare.hpp>
#include <KlayGE/HDRPostProcess.hpp>

#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/InputFactory.hpp>

#include <vector>
#include <sstream>
#include <ctime>
#include <iostream>
#include <boost/tuple/tuple.hpp>
#include <boost/bind.hpp>

#include "OceanSimulator.hpp"
#include "Ocean.hpp"

using namespace std;
using namespace KlayGE;

namespace
{
	class RenderTerrain : public InfTerrainRenderable
	{
	public:
		RenderTerrain(float base_level, float strength)
			: InfTerrainRenderable(L"Terrain")
		{
			RenderFactory& rf = Context::Instance().RenderFactoryInstance();
			technique_ = rf.LoadEffect("Terrain.fxml")->TechniqueByName("Terrain");

			this->SetStretch(strength);
			this->SetBaseLevel(base_level);
		}

		void SunDirection(float3 const & dir)
		{
			*(technique_->Effect().ParameterByName("sun_dir")) = -dir;
		}

		void SunColor(Color const & clr)
		{
			*(technique_->Effect().ParameterByName("sun_color")) = float3(clr.r(), clr.g(), clr.b());
		}

		void FogColor(Color const & clr)
		{
			*(technique_->Effect().ParameterByName("fog_color")) = float3(clr.r(), clr.g(), clr.b());
		}

		void ReflectionPass(bool ref)
		{
			ref_ = ref;
		}

		void ReflectionPlane(Plane const & plane)
		{
			plane_ = plane;
		}

		void OnRenderBegin()
		{
			App3DFramework const & app = Context::Instance().AppInstance();
			Camera const & camera = app.ActiveCamera();

			float4x4 const & view = camera.ViewMatrix();
			float4x4 proj = camera.ProjMatrix();

			float3 look_at_vec = float3(camera.LookAt().x() - camera.EyePos().x(), 0, camera.LookAt().z() - camera.EyePos().z());
			if (MathLib::dot(look_at_vec, look_at_vec) < 1e-6f)
			{
				look_at_vec = float3(0, 0, 1);
			}
			float4x4 virtual_view = MathLib::look_at_lh(camera.EyePos(), camera.EyePos() + look_at_vec);
			float4x4 inv_virtual_view = MathLib::inverse(virtual_view);

			if (ref_)
			{
				MathLib::oblique_clipping(proj, 
					MathLib::mul(plane_, MathLib::transpose(MathLib::inverse(view))));
				Context::Instance().RenderFactoryInstance().RenderEngineInstance().AdjustPerspectiveMatrix(proj);
			}

			float4x4 vp = view * proj;
			*(technique_->Effect().ParameterByName("mvp")) = vp;
			*(technique_->Effect().ParameterByName("inv_virtual_view")) = inv_virtual_view;
			*(technique_->Effect().ParameterByName("eye_pos")) = camera.EyePos();
		}

	private:
		bool ref_;
		Plane plane_;
	};

	class TerrainObject : public InfTerrainSceneObject
	{
	public:
		TerrainObject()
		{
			base_level_ = -10;
			strength_ = 50;

			renderable_ = MakeSharedPtr<RenderTerrain>(base_level_, strength_);
		}

		void SunDirection(float3 const & dir)
		{
			checked_pointer_cast<RenderTerrain>(renderable_)->SunDirection(dir);
		}

		void SunColor(Color const & clr)
		{
			checked_pointer_cast<RenderTerrain>(renderable_)->SunColor(clr);
		}

		void FogColor(Color const & clr)
		{
			checked_pointer_cast<RenderTerrain>(renderable_)->FogColor(clr);
		}

		void ReflectionPass(bool ref)
		{
			checked_pointer_cast<RenderTerrain>(renderable_)->ReflectionPass(ref);
		}

		void ReflectionPlane(Plane const & plane)
		{
			checked_pointer_cast<RenderTerrain>(renderable_)->ReflectionPlane(plane);
		}
	};

	class RenderOcean : public InfTerrainRenderable
	{
	public:
		RenderOcean(float base_level, float strength)
			: InfTerrainRenderable(L"Ocean")
		{
			RenderFactory& rf = Context::Instance().RenderFactoryInstance();
			technique_ = rf.LoadEffect("Ocean.fxml")->TechniqueByName("Ocean");

			this->SetStretch(strength);
			this->SetBaseLevel(base_level);
		}

		void SunDirection(float3 const & dir)
		{
			*(technique_->Effect().ParameterByName("sun_dir")) = -dir;
		}

		void SunColor(Color const & clr)
		{
			*(technique_->Effect().ParameterByName("sun_color")) = float3(clr.r(), clr.g(), clr.b());
		}

		void FogColor(Color const & clr)
		{
			*(technique_->Effect().ParameterByName("fog_color")) = float3(clr.r(), clr.g(), clr.b());
		}

		void PatchLength(float patch_length)
		{
			*(technique_->Effect().ParameterByName("patch_length")) = patch_length;
		}

		void DisplacementMap(TexturePtr const & tex0, TexturePtr const & tex1)
		{
			*(technique_->Effect().ParameterByName("displacement_tex_0")) = tex0;
			*(technique_->Effect().ParameterByName("displacement_tex_1")) = tex1;
		}

		void GradientMap(TexturePtr const & tex0, TexturePtr const & tex1)
		{
			*(technique_->Effect().ParameterByName("gradient_tex_0")) = tex0;
			*(technique_->Effect().ParameterByName("gradient_tex_1")) = tex1;
		}

		void DisplacementMapArray(TexturePtr const & tex)
		{
			*(technique_->Effect().ParameterByName("displacement_tex_array")) = tex;
		}

		void GradientMapArray(TexturePtr const & tex)
		{
			*(technique_->Effect().ParameterByName("gradient_tex_array")) = tex;
		}

		void Frames(int2 const & frames)
		{
			*(technique_->Effect().ParameterByName("frames")) = frames;
		}

		void InterpolateFrac(float frac)
		{
			*(technique_->Effect().ParameterByName("interpolate_frac")) = frac;
		}

		void RefractionTex(TexturePtr const & tex)
		{
			*(technique_->Effect().ParameterByName("refraction_tex")) = tex;
		}

		void ReflectionTex(TexturePtr const & tex)
		{
			*(technique_->Effect().ParameterByName("reflection_tex")) = tex;
		}

		void OnRenderBegin()
		{
			InfTerrainRenderable::OnRenderBegin();

			RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();
			*(technique_->Effect().ParameterByName("flipping")) = static_cast<int32_t>(re.CurFrameBuffer()->RequiresFlipping() ? -1 : +1);
		}
	};

	class OceanObject : public InfTerrainSceneObject
	{
	public:
		OceanObject()
		{
			RenderFactory& rf = Context::Instance().RenderFactoryInstance();
			RenderEngine& re = rf.RenderEngineInstance();

			RenderDeviceCaps const & caps = re.DeviceCaps();
			cs_simulate_ = caps.cs_support;

			base_level_ = 0;
			strength_ = 10;

			renderable_ = MakeSharedPtr<RenderOcean>(base_level_, strength_);

			ocean_plane_ = MathLib::from_point_normal(float3(0, base_level_, 0), float3(0, 1, 0));
			reflect_mat_ = MathLib::reflect(ocean_plane_);

			// The size of displacement map.
			ocean_param_.dmap_dim			= 512;
			// The side length (world space) of square sized patch
			ocean_param_.patch_length		= 42;

			ocean_param_.time_peroid		= 3;
			ocean_param_.num_frames			= 24;

			// Adjust this parameter to control the simulation speed
			ocean_param_.time_scale			= 0.8f;
			// A scale to control the amplitude. Not the world space height
			ocean_param_.wave_amplitude		= 0.003f;
			// 2D wind direction. No need to be normalized
			// The bigger the wind speed, the larger scale of wave crest.
			// But the wave scale can be no larger than patch_length
			ocean_param_.wind_speed			= float2(0.8f, 0.6f) * 6.0f;
			// Damp out the components opposite to wind direction.
			// The smaller the value, the higher wind dependency
			ocean_param_.wind_dependency	= 0.1f;
			// Control the scale of horizontal movement. Higher value creates
			// pointy crests.
			ocean_param_.choppy_scale		= 1.1f;

			if (cs_simulate_)
			{
				ocean_simulator_ = MakeSharedPtr<OceanSimulator>();
			}

			use_tex_array_ = re.DeviceCaps().max_texture_array_length >= ocean_param_.num_frames;

			Texture::TextureType disp_type;
			uint32_t disp_width, disp_height, disp_depth;
			uint32_t disp_num_mipmaps;
			uint32_t disp_array_size;
			ElementFormat disp_format;
			std::vector<ElementInitData> disp_init_data;
			std::vector<uint8_t> disp_data_block;
			LoadTexture("OceanWave.7z//OceanDisplacement.dds", disp_type, disp_width, disp_height, disp_depth, disp_num_mipmaps, disp_array_size,
				disp_format, disp_init_data, disp_data_block);

			Texture::TextureType grad_type;
			uint32_t grad_width, grad_height, grad_depth;
			uint32_t grad_num_mipmaps;
			uint32_t grad_array_size;
			ElementFormat grad_format;
			std::vector<ElementInitData> grad_init_data;
			std::vector<uint8_t> grad_data_block;
			LoadTexture("OceanWave.7z//OceanGradient.dds", grad_type, grad_width, grad_height, grad_depth, grad_num_mipmaps, grad_array_size,
				grad_format, grad_init_data, grad_data_block);

			bool use_load_tex;
			if ((disp_array_size == ocean_param_.num_frames) && (disp_width == static_cast<uint32_t>(ocean_param_.dmap_dim)) && (disp_height == disp_width)
				&& (grad_array_size == ocean_param_.num_frames) && (grad_width == static_cast<uint32_t>(ocean_param_.dmap_dim)) && (grad_height == grad_width))
			{
				use_load_tex = true;
				dirty_ = false;
			}
			else
			{
				use_load_tex = false;
				dirty_ = true;
			}

			if (use_tex_array_)
			{
				ElementInitData* did = NULL;
				ElementInitData* gid = NULL;
				if (use_load_tex)
				{
					did = &disp_init_data[0];
					gid = &grad_init_data[0];
				}

				displacement_tex_array_ = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
					1, ocean_param_.num_frames, EF_ABGR16F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, did);

				TexturePtr gta = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
					1, ocean_param_.num_frames, EF_GR8, 1, 0, EAH_CPU_Read | EAH_CPU_Write, gid);
				gradient_tex_array_ = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
					0, ocean_param_.num_frames, EF_GR8, 1, 0, EAH_GPU_Read | EAH_GPU_Write | EAH_Generate_Mips, NULL);
				for (uint32_t i = 0; i < ocean_param_.num_frames; ++ i)
				{
					gta->CopyToTextureArray(*gradient_tex_array_, 0, ocean_param_.dmap_dim, ocean_param_.dmap_dim, 0, 0, i,
						ocean_param_.dmap_dim, ocean_param_.dmap_dim, 0, 0, i);
				}
				gradient_tex_array_->BuildMipSubLevels();
			}
			else
			{
				displacement_tex_.resize(ocean_param_.num_frames);
				gradient_tex_.resize(ocean_param_.num_frames);
				for (uint32_t i = 0; i < ocean_param_.num_frames; ++ i)
				{
					ElementInitData* did = NULL;
					ElementInitData* gid = NULL;
					if (use_load_tex)
					{
						did = &disp_init_data[i * disp_num_mipmaps];
						gid = &grad_init_data[i * grad_num_mipmaps];
					}

					displacement_tex_[i] = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
						1, 1, EF_ABGR16F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, did);

					TexturePtr gta = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
						1, 1, EF_GR8, 1, 0, EAH_CPU_Read | EAH_CPU_Write, gid);
					gradient_tex_[i] = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
						0, 1, EF_GR8, 1, 0, EAH_GPU_Read | EAH_GPU_Write | EAH_Generate_Mips, NULL);
					gta->CopyToTexture2D(*gradient_tex_[i], 0, ocean_param_.dmap_dim, ocean_param_.dmap_dim,
						0, 0, ocean_param_.dmap_dim, ocean_param_.dmap_dim, 0, 0);
					gradient_tex_[i]->BuildMipSubLevels();
				}
			}
		}

		void SunDirection(float3 const & dir)
		{
			checked_pointer_cast<RenderOcean>(renderable_)->SunDirection(dir);
		}

		void SunColor(Color const & clr)
		{
			checked_pointer_cast<RenderOcean>(renderable_)->SunColor(clr);
		}

		void FogColor(Color const & clr)
		{
			checked_pointer_cast<RenderOcean>(renderable_)->FogColor(clr);
		}

		Plane const & OceanPlane() const
		{
			return ocean_plane_;
		}

		void Update()
		{
			if (dirty_)
			{
				checked_pointer_cast<RenderOcean>(renderable_)->PatchLength(ocean_param_.patch_length);

				if (cs_simulate_)
				{
					ocean_simulator_->Parameters(ocean_param_);

					for (uint32_t i = 0; i < ocean_param_.num_frames; ++ i)
					{
						ocean_simulator_->Update(i);

						TexturePtr const & sim_disp_tex = ocean_simulator_->DisplacementTex();
						TexturePtr const & sim_grad_tex = ocean_simulator_->GradientTex();

						if (use_tex_array_)
						{
							sim_disp_tex->CopyToTextureArray(*displacement_tex_array_, 0,
								sim_disp_tex->Width(0), sim_disp_tex->Height(0), 0, 0, i,
								sim_disp_tex->Width(0), sim_disp_tex->Height(0), 0, 0, 0);
							for (uint32_t l = 0; l < gradient_tex_array_->NumMipMaps(); ++ l)
							{
								sim_grad_tex->CopyToTextureArray(*gradient_tex_array_, l,
									sim_grad_tex->Width(l), sim_grad_tex->Height(l), 0, 0, i,
									sim_grad_tex->Width(l), sim_grad_tex->Height(l), 0, 0, 0);
							}
						}
						else
						{
							sim_disp_tex->CopyToTexture(*displacement_tex_[i]);
							sim_grad_tex->CopyToTexture(*gradient_tex_[i]);
						}
					}

					/*if (use_tex_array_)
					{
						SaveTexture(displacement_tex_array_, "OceanDisplacement.dds");

						RenderFactory& rf = Context::Instance().RenderFactoryInstance();
						TexturePtr gta = rf.MakeTexture2D(ocean_param_.dmap_dim, ocean_param_.dmap_dim,
							1, ocean_param_.num_frames, EF_GR8, 1, 0, EAH_CPU_Read | EAH_CPU_Write, NULL);
						for (uint32_t i = 0; i < ocean_param_.num_frames; ++ i)
						{
							gradient_tex_array_->CopyToTextureArray(*gta, 0, ocean_param_.dmap_dim, ocean_param_.dmap_dim, 0, 0, i,
								ocean_param_.dmap_dim, ocean_param_.dmap_dim, 0, 0, i);
						}
						SaveTexture(gta, "OceanGradient.dds");
					}*/
				}

				dirty_ = false;
			}

			InfTerrainSceneObject::Update();

			float t = static_cast<float>(timer_.elapsed() * ocean_param_.time_scale) / ocean_param_.time_peroid;
			float frame = (t - floor(t)) * ocean_param_.num_frames;
			int frame0 = static_cast<int>(frame);
			int frame1 = frame0 + 1;
			checked_pointer_cast<RenderOcean>(renderable_)->InterpolateFrac(frame - frame0);
			frame0 %= ocean_param_.num_frames;
			frame1 %= ocean_param_.num_frames;
			if (use_tex_array_)
			{
				checked_pointer_cast<RenderOcean>(renderable_)->Frames(int2(frame0, frame1));
				checked_pointer_cast<RenderOcean>(renderable_)->DisplacementMapArray(displacement_tex_array_);
				checked_pointer_cast<RenderOcean>(renderable_)->GradientMapArray(gradient_tex_array_);
			}
			else
			{
				checked_pointer_cast<RenderOcean>(renderable_)->DisplacementMap(displacement_tex_[frame0], displacement_tex_[frame1]);
				checked_pointer_cast<RenderOcean>(renderable_)->GradientMap(gradient_tex_[frame0], gradient_tex_[frame1]);
			}
		}

		void RefractionTex(TexturePtr const & tex)
		{
			checked_pointer_cast<RenderOcean>(renderable_)->RefractionTex(tex);
		}

		void ReflectionTex(TexturePtr const & tex)
		{
			checked_pointer_cast<RenderOcean>(renderable_)->ReflectionTex(tex);
		}

		void ReflectViewParams(float3& reflect_eye, float3& reflect_at, float3& reflect_up,
			float3 const & eye, float3 const & at, float3 const & up)
		{
			reflect_eye = MathLib::transform_coord(eye, reflect_mat_);
			reflect_at = MathLib::transform_coord(at, reflect_mat_);
			reflect_up = MathLib::transform_normal(up, reflect_mat_);
			reflect_up *= -1.0f;
		}

		int DMapDim() const
		{
			return ocean_param_.dmap_dim;
		}
		void DMapDim(int dmap_dim)
		{
			ocean_param_.dmap_dim = dmap_dim;
			dirty_ = true;
		}

		float PatchLength() const
		{
			return ocean_param_.patch_length;
		}
		void PatchLength(float patch_length)
		{
			ocean_param_.patch_length = patch_length;
			dirty_ = true;
		}

		float TimeScale() const
		{
			return ocean_param_.time_scale;
		}
		void TimeScale(float time_scale)
		{
			ocean_param_.time_scale = time_scale;
			dirty_ = true;
		}

		float WaveAmplitude() const
		{
			return ocean_param_.wave_amplitude;
		}
		void WaveAmplitude(float amp)
		{
			ocean_param_.wave_amplitude = amp;
			dirty_ = true;
		}

		float WindSpeedX() const
		{
			return ocean_param_.wind_speed.x();
		}
		void WindSpeedX(float speed)
		{
			ocean_param_.wind_speed.x() = speed;
			dirty_ = true;
		}

		float WindSpeedY() const
		{
			return ocean_param_.wind_speed.y();
		}
		void WindSpeedY(float speed)
		{
			ocean_param_.wind_speed.y() = speed;
			dirty_ = true;
		}

		float WindDependency() const
		{
			return ocean_param_.wind_dependency;
		}
		void WindDependency(float dep)
		{
			ocean_param_.wind_dependency = dep;
			dirty_ = true;
		}

		float ChoppyScale() const
		{
			return ocean_param_.choppy_scale;
		}
		void ChoppyScale(float choppy)
		{
			ocean_param_.choppy_scale = choppy;
			dirty_ = true;
		}

	private:
		bool cs_simulate_;

		OceanParameter ocean_param_;
		bool dirty_;

		Plane ocean_plane_;
		float4x4 reflect_mat_;

		boost::shared_ptr<OceanSimulator> ocean_simulator_;

		bool use_tex_array_;
		std::vector<TexturePtr> displacement_tex_;
		std::vector<TexturePtr> gradient_tex_;
		TexturePtr displacement_tex_array_;
		TexturePtr gradient_tex_array_;

		Timer timer_;
	};

	
	class RenderableFoggyHDRSkyBox : public RenderableHDRSkyBox
	{
	public:
		RenderableFoggyHDRSkyBox()
		{
			RenderFactory& rf = Context::Instance().RenderFactoryInstance();
			this->Technique(rf.LoadEffect("Ocean.fxml")->TechniqueByName("FoggySkyBox"));
		}
		
		void FogColor(Color const & clr)
		{
			*(technique_->Effect().ParameterByName("fog_color")) = float3(clr.r(), clr.g(), clr.b());
		}
	};

	class SceneObjectFoggyHDRSkyBox : public SceneObjectHDRSkyBox
	{
	public:
		SceneObjectFoggyHDRSkyBox(uint32_t attrib = 0)
			: SceneObjectHDRSkyBox(attrib)
		{
			renderable_ = MakeSharedPtr<RenderableFoggyHDRSkyBox>();
		}

		void FogColor(Color const & clr)
		{
			checked_pointer_cast<RenderableFoggyHDRSkyBox>(renderable_)->FogColor(clr);
		}
	};


	enum
	{
		Exit,
	};

	InputActionDefine actions[] = 
	{
		InputActionDefine(Exit, KS_Escape),
	};
}


int main()
{
	ResLoader::Instance().AddPath("../Samples/media/Common");

	Context::Instance().LoadCfg("KlayGE.cfg");

	OceanApp app;
	app.Create();
	app.Run();

	return 0;
}

OceanApp::OceanApp()
				: App3DFramework("Ocean")
{
	ResLoader::Instance().AddPath("../Samples/media/Ocean");
}

bool OceanApp::ConfirmDevice() const
{
	RenderFactory& rf = Context::Instance().RenderFactoryInstance();
	RenderEngine& re = rf.RenderEngineInstance();
	RenderDeviceCaps const & caps = re.DeviceCaps();
	if (caps.max_shader_model < 3)
	{
		return false;
	}
	return true;
}

void OceanApp::InitObjects()
{
	this->LookAt(float3(0, 20, 0), float3(0, 19.8f, 1));
	this->Proj(0.01f, 3000);

	// ��������
	font_ = Context::Instance().RenderFactoryInstance().MakeFont("gkai00mp.kfont");

	TexturePtr skybox_y_tex = LoadTexture("DH001cross_y.dds", EAH_GPU_Read)();
	TexturePtr skybox_c_tex = LoadTexture("DH001cross_c.dds", EAH_GPU_Read)();

	terrain_ = MakeSharedPtr<TerrainObject>();
	terrain_->AddToSceneManager();
	ocean_ = MakeSharedPtr<OceanObject>();
	ocean_->AddToSceneManager();
	sun_flare_ = MakeSharedPtr<LensFlareSceneObject>();
	checked_pointer_cast<LensFlareSceneObject>(sun_flare_)->Direction(float3(-0.267835f, 0.0517653f, 0.960315f));
	sun_flare_->AddToSceneManager();

	Color sun_color(1, 0.7f, 0.5f, 1);
	Color fog_color(0.61f, 0.52f, 0.62f, 1);

	checked_pointer_cast<TerrainObject>(terrain_)->SunDirection(checked_pointer_cast<LensFlareSceneObject>(sun_flare_)->Direction());
	checked_pointer_cast<TerrainObject>(terrain_)->SunColor(sun_color);
	checked_pointer_cast<TerrainObject>(terrain_)->FogColor(fog_color);
	checked_pointer_cast<OceanObject>(ocean_)->SunDirection(checked_pointer_cast<LensFlareSceneObject>(sun_flare_)->Direction());
	checked_pointer_cast<OceanObject>(ocean_)->SunColor(sun_color);
	checked_pointer_cast<OceanObject>(ocean_)->FogColor(fog_color);

	checked_pointer_cast<TerrainObject>(terrain_)->ReflectionPlane(checked_pointer_cast<OceanObject>(ocean_)->OceanPlane());

	sky_box_ = MakeSharedPtr<SceneObjectFoggyHDRSkyBox>();
	checked_pointer_cast<SceneObjectFoggyHDRSkyBox>(sky_box_)->CompressedCubeMap(skybox_y_tex, skybox_c_tex);
	checked_pointer_cast<SceneObjectFoggyHDRSkyBox>(sky_box_)->FogColor(fog_color);
	sky_box_->AddToSceneManager();

	fpcController_.Scalers(0.05f, 1.0f);

	InputEngine& inputEngine(Context::Instance().InputFactoryInstance().InputEngineInstance());
	InputActionMap actionMap;
	actionMap.AddActions(actions, actions + sizeof(actions) / sizeof(actions[0]));

	action_handler_t input_handler = MakeSharedPtr<input_signal>();
	input_handler->connect(boost::bind(&OceanApp::InputHandler, this, _1, _2));
	inputEngine.ActionMap(actionMap, input_handler, true);

	copy_pp_ = LoadPostProcess(ResLoader::Instance().Load("Copy.ppml"), "copy");
	hdr_pp_ = MakeSharedPtr<HDRPostProcess>();

	RenderFactory& rf = Context::Instance().RenderFactoryInstance();
	refraction_fb_ = rf.MakeFrameBuffer();
	refraction_fb_->GetViewport().camera = rf.RenderEngineInstance().CurFrameBuffer()->GetViewport().camera;

	Camera& scene_camera = this->ActiveCamera();
	reflection_fb_ = rf.MakeFrameBuffer();
	reflection_fb_->GetViewport().camera->ProjParams(scene_camera.FOV(), scene_camera.Aspect(),
			scene_camera.NearPlane(), scene_camera.FarPlane());

	blur_y_ = MakeSharedPtr<BlurYPostProcess<SeparableGaussianFilterPostProcess> >(8, 1.0f);

	final_fb_ = rf.MakeFrameBuffer();
	final_fb_->GetViewport().camera = rf.RenderEngineInstance().CurFrameBuffer()->GetViewport().camera;

	UIManager::Instance().Load(ResLoader::Instance().Load("Ocean.uiml"));
	dialog_params_ = UIManager::Instance().GetDialog("Parameters");
	id_dmap_dim_static_ = dialog_params_->IDFromName("DMapDimStatic");
	id_dmap_dim_slider_ = dialog_params_->IDFromName("DMapDimSlider");
	id_patch_length_static_ = dialog_params_->IDFromName("PatchLengthStatic");
	id_patch_length_slider_ = dialog_params_->IDFromName("PatchLengthSlider");
	id_time_scale_static_ = dialog_params_->IDFromName("TimeScaleStatic");
	id_time_scale_slider_ = dialog_params_->IDFromName("TimeScaleSlider");
	id_wave_amplitude_static_ = dialog_params_->IDFromName("WaveAmplitudeStatic");
	id_wave_amplitude_slider_ = dialog_params_->IDFromName("WaveAmplitudeSlider");
	id_wind_speed_x_static_ = dialog_params_->IDFromName("WindSpeedXStatic");
	id_wind_speed_x_slider_ = dialog_params_->IDFromName("WindSpeedXSlider");
	id_wind_speed_y_static_ = dialog_params_->IDFromName("WindSpeedYStatic");
	id_wind_speed_y_slider_ = dialog_params_->IDFromName("WindSpeedYSlider");
	id_wind_dependency_static_ = dialog_params_->IDFromName("WindDependencyStatic");
	id_wind_dependency_slider_ = dialog_params_->IDFromName("WindDependencySlider");
	id_choppy_scale_static_ = dialog_params_->IDFromName("ChoppyScaleStatic");
	id_choppy_scale_slider_ = dialog_params_->IDFromName("ChoppyScaleSlider");
	id_fps_camera_ = dialog_params_->IDFromName("FPSCamera");

	dialog_params_->Control<UISlider>(id_dmap_dim_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::DMapDimChangedHandler, this, _1));
	this->DMapDimChangedHandler(*dialog_params_->Control<UISlider>(id_dmap_dim_slider_));

	dialog_params_->Control<UISlider>(id_patch_length_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::PatchLengthChangedHandler, this, _1));
	this->PatchLengthChangedHandler(*dialog_params_->Control<UISlider>(id_patch_length_slider_));

	dialog_params_->Control<UISlider>(id_time_scale_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::TimeScaleChangedHandler, this, _1));
	this->TimeScaleChangedHandler(*dialog_params_->Control<UISlider>(id_time_scale_slider_));

	dialog_params_->Control<UISlider>(id_wave_amplitude_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::WaveAmplitudeChangedHandler, this, _1));
	this->WaveAmplitudeChangedHandler(*dialog_params_->Control<UISlider>(id_wave_amplitude_slider_));

	dialog_params_->Control<UISlider>(id_wind_speed_x_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::WindSpeedXChangedHandler, this, _1));
	this->WindSpeedXChangedHandler(*dialog_params_->Control<UISlider>(id_wind_speed_x_slider_));

	dialog_params_->Control<UISlider>(id_wind_speed_y_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::WindSpeedYChangedHandler, this, _1));
	this->WindSpeedYChangedHandler(*dialog_params_->Control<UISlider>(id_wind_speed_y_slider_));

	dialog_params_->Control<UISlider>(id_wind_dependency_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::WindDependencyChangedHandler, this, _1));
	this->WindDependencyChangedHandler(*dialog_params_->Control<UISlider>(id_wind_dependency_slider_));

	dialog_params_->Control<UISlider>(id_choppy_scale_slider_)->OnValueChangedEvent().connect(boost::bind(&OceanApp::ChoppyScaleChangedHandler, this, _1));
	this->ChoppyScaleChangedHandler(*dialog_params_->Control<UISlider>(id_choppy_scale_slider_));

	dialog_params_->Control<UICheckBox>(id_fps_camera_)->OnChangedEvent().connect(boost::bind(&OceanApp::FPSCameraHandler, this, _1));
}

void OceanApp::OnResize(uint32_t width, uint32_t height)
{
	App3DFramework::OnResize(width, height);

	RenderFactory& rf = Context::Instance().RenderFactoryInstance();

	RenderViewPtr ds_view = rf.Make2DDepthStencilRenderView(width, height, EF_D24S8, 1, 0);

	refraction_tex_ = rf.MakeTexture2D(width, height, 1, 1, EF_ABGR16F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, NULL);
	refraction_fb_->Attach(FrameBuffer::ATT_Color0, rf.Make2DRenderView(*refraction_tex_, 0, 0));
	refraction_fb_->Attach(FrameBuffer::ATT_DepthStencil, ds_view);

	reflection_tex_ = rf.MakeTexture2D(width / 2, height / 2, 1, 1, EF_ABGR16F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, NULL);
	reflection_blur_tex_ = rf.MakeTexture2D(width / 2, height / 2, 1, 1, EF_ABGR16F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, NULL);
	reflection_fb_->Attach(FrameBuffer::ATT_Color0, rf.Make2DRenderView(*reflection_tex_, 0, 0));
	reflection_fb_->Attach(FrameBuffer::ATT_DepthStencil, rf.Make2DDepthStencilRenderView(width / 2, height / 2, EF_D16, 1, 0));
	reflection_fb_->GetViewport().left = 0;
	reflection_fb_->GetViewport().top = 0;
	reflection_fb_->GetViewport().width = width / 2;
	reflection_fb_->GetViewport().height = height / 2;

	blur_y_->InputPin(0, reflection_tex_);
	blur_y_->OutputPin(0, reflection_blur_tex_);

	try
	{
		final_tex_ = rf.MakeTexture2D(width, height, 1, 1, EF_B10G11R11F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, NULL);
		final_fb_->Attach(FrameBuffer::ATT_Color0, rf.Make2DRenderView(*final_tex_, 0, 0));
	}
	catch (...)
	{
		final_tex_ = rf.MakeTexture2D(width, height, 1, 1, EF_ABGR16F, 1, 0, EAH_GPU_Read | EAH_GPU_Write, NULL);
		final_fb_->Attach(FrameBuffer::ATT_Color0, rf.Make2DRenderView(*final_tex_, 0, 0));
	}
	final_fb_->Attach(FrameBuffer::ATT_DepthStencil, ds_view);

	copy_pp_->InputPin(0, refraction_tex_);
	copy_pp_->OutputPin(0, final_tex_);
	hdr_pp_->InputPin(0, final_tex_);

	checked_pointer_cast<OceanObject>(ocean_)->RefractionTex(refraction_tex_);
	checked_pointer_cast<OceanObject>(ocean_)->ReflectionTex(reflection_blur_tex_);

	UIManager::Instance().SettleCtrls(width, height);
}

void OceanApp::InputHandler(InputEngine const & /*sender*/, InputAction const & action)
{
	switch (action.first)
	{
	case Exit:
		this->Quit();
		break;
	}
}

void OceanApp::DMapDimChangedHandler(UISlider const & sender)
{
	int dmap_dim = 1UL << (3 * sender.GetValue());

	std::wostringstream stream;
	stream << L"DMap dim: " << dmap_dim;
	dialog_params_->Control<UIStatic>(id_dmap_dim_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->DMapDim(dmap_dim);
}

void OceanApp::PatchLengthChangedHandler(UISlider const & sender)
{
	float patch_length = sender.GetValue() * 0.5f;

	std::wostringstream stream;
	stream << L"Patch length: " << patch_length;
	dialog_params_->Control<UIStatic>(id_patch_length_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->PatchLength(patch_length);
}

void OceanApp::TimeScaleChangedHandler(UISlider const & sender)
{
	float time_scale = sender.GetValue() * 0.1f;

	std::wostringstream stream;
	stream << L"Time scale: " << time_scale;
	dialog_params_->Control<UIStatic>(id_time_scale_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->TimeScale(time_scale);
}

void OceanApp::WaveAmplitudeChangedHandler(UISlider const & sender)
{
	float wave_amp = sender.GetValue() * 0.0001f;

	std::wostringstream stream;
	stream << L"Wave amplitude: " << wave_amp;
	dialog_params_->Control<UIStatic>(id_wave_amplitude_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->WaveAmplitude(wave_amp);
}

void OceanApp::WindSpeedXChangedHandler(UISlider const & sender)
{
	float wind_speed = sender.GetValue() * 0.05f;

	std::wostringstream stream;
	stream << L"Wind speed X: " << wind_speed;
	dialog_params_->Control<UIStatic>(id_wind_speed_x_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->WindSpeedX(wind_speed);
}

void OceanApp::WindSpeedYChangedHandler(UISlider const & sender)
{
	float wind_speed = sender.GetValue() * 0.05f;

	std::wostringstream stream;
	stream << L"Wind speed Y: " << wind_speed;
	dialog_params_->Control<UIStatic>(id_wind_speed_y_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->WindSpeedY(wind_speed);
}

void OceanApp::WindDependencyChangedHandler(UISlider const & sender)
{
	float dep = sender.GetValue() * 0.01f;

	std::wostringstream stream;
	stream << L"Wind dependency: " << dep;
	dialog_params_->Control<UIStatic>(id_wind_dependency_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->WindDependency(dep);
}

void OceanApp::ChoppyScaleChangedHandler(UISlider const & sender)
{
	float choppy = sender.GetValue() * 0.1f;

	std::wostringstream stream;
	stream << L"Choppy scale: " << choppy;
	dialog_params_->Control<UIStatic>(id_choppy_scale_static_)->SetText(stream.str());

	checked_pointer_cast<OceanObject>(ocean_)->ChoppyScale(choppy);
}

void OceanApp::FPSCameraHandler(UICheckBox const & sender)
{
	if (sender.GetChecked())
	{
		fpcController_.AttachCamera(this->ActiveCamera());
	}
	else
	{
		fpcController_.DetachCamera();
	}
}

void OceanApp::DoUpdateOverlay()
{
	UIManager::Instance().Render();

	SceneManager& sceneMgr(Context::Instance().SceneManagerInstance());

	std::wostringstream stream;
	stream << this->FPS();

	font_->RenderText(0, 0, Color(1, 1, 0, 1), L"Ocean", 16);
	font_->RenderText(0, 18, Color(1, 1, 0, 1), stream.str(), 16);

	stream.str(L"");
	stream << sceneMgr.NumRenderablesRendered() << " Renderables "
		<< sceneMgr.NumPrimitivesRendered() << " Primitives "
		<< sceneMgr.NumVerticesRendered() << " Vertices";
	font_->RenderText(0, 36, Color(1, 1, 1, 1), stream.str(), 16);
}

uint32_t OceanApp::DoUpdate(uint32_t pass)
{
	RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();

	switch (pass)
	{
	case 0:
		re.BindFrameBuffer(refraction_fb_);
		re.CurFrameBuffer()->Attached(FrameBuffer::ATT_DepthStencil)->ClearDepth(1);
		checked_pointer_cast<TerrainObject>(terrain_)->ReflectionPass(false);
		terrain_->Visible(true);
		sky_box_->Visible(true);
		ocean_->Visible(false);
		sun_flare_->Visible(false);
		return App3DFramework::URV_Need_Flush;

	case 1:
		{
			Camera& scene_camera = this->ActiveCamera();

			re.BindFrameBuffer(reflection_fb_);
			re.CurFrameBuffer()->Attached(FrameBuffer::ATT_DepthStencil)->ClearDepth(1);
			checked_pointer_cast<TerrainObject>(terrain_)->ReflectionPass(true);
			terrain_->Visible(true);
			sky_box_->Visible(true);
			ocean_->Visible(false);
			sun_flare_->Visible(false);

			float3 reflect_eye, reflect_at, reflect_up;
			checked_pointer_cast<OceanObject>(ocean_)->ReflectViewParams(reflect_eye, reflect_at, reflect_up,
				scene_camera.EyePos(), scene_camera.LookAt(), scene_camera.UpVec());
			reflection_fb_->GetViewport().camera->ViewParams(reflect_eye, reflect_at, reflect_up);
		}

		return App3DFramework::URV_Need_Flush;

	case 2:
		blur_y_->Apply();
		re.BindFrameBuffer(final_fb_);
		copy_pp_->Apply();
		terrain_->Visible(false);
		sky_box_->Visible(false);
		ocean_->Visible(true);
		sun_flare_->Visible(true);

		return App3DFramework::URV_Need_Flush;

	default:
		re.BindFrameBuffer(FrameBufferPtr());
		re.CurFrameBuffer()->Attached(FrameBuffer::ATT_DepthStencil)->ClearDepth(1);
		hdr_pp_->Apply();

		return App3DFramework::URV_Flushed | App3DFramework::URV_Finished;
	}
}
