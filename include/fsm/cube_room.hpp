#ifndef __FSM_CUBE_ROOM_HPP
#define __FSM_CUBE_ROOM_HPP

#include <fsm/common.hpp>
#include <fsm/laser.hpp>
#include <fsm/liquid.hpp>
#include <fsm/slab.hpp>

#include <core/buffer.hpp>
#include <core/framebuffer.hpp>
#include <core/shader.hpp>
#include <core/texture.hpp>
#include <core/vertex_array.hpp>
#include <data/subplane.hpp>
#include <lang/primtypes.hpp>
#include <scene/freefly.hpp>
#include <sync/parts_fsm.hpp>
#include <tech/framebuffer_copy.hpp>
#include <tech/post_process.hpp>
#include <tech/deferred_renderer.hpp>

class CubeRoom : public sky::sync::PartState {
  /* common */
  sky::ushort _width, _height;
  sky::tech::DefaultFramebufferCopy _fbCopier;
  sky::scene::Freefly const &_freefly;
  sky::tech::DeferredRenderer &_drenderer;
  sky::scene::MaterialManager &_matmgr;
  sky::glyph::StringRenderer &_stringRenderer;
  sky::core::Program::Uniform _matmgrProjIndex;
  sky::core::Program::Uniform _matmgrViewIndex;
  sky::core::Program::Uniform _matmgrLColorIndex;
  sky::core::Program::Uniform _matmgrLPosIndex;
  sky::scene::Material _matPlastic;

  sky::core::Texture _offTex;
  sky::core::Framebuffer _offFB;
  sky::core::Renderbuffer _offRB;
  sky::tech::TemporalPostProcess _fadePP;

  Slab _slab;
  Liquid _liquid;
  Laser _laser;

  void _init_materials(sky::ushort width, sky::ushort height);
  void _init_offscreen(sky::ushort width, sky::ushort height);
  void _draw_texts(float t) const;

public :
  CubeRoom(sky::ushort width, sky::ushort height, Common &common, sky::scene::Freefly const &freefly);
  ~CubeRoom(void) = default;

  void run(float time) override;
};

#endif /* guard */

