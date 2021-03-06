#include <core/state.hpp>
#include <fsm/common.hpp>
#include <fsm/cube_room.hpp>
#include <math/common.hpp>
#include <math/matrix.hpp>
#include <math/quaternion.hpp>
#include <misc/log.hpp>

using namespace std;
using namespace sky;
using namespace core;
using namespace math;
using namespace misc;
using namespace scene;
using namespace tech;

namespace {
  ushort const LASER_TESS_LEVEL = 13;
  float  const LASER_HHEIGHT    = 0.15f;
  float  const SLAB_SIZE        = 1.f;
  float  const SLAB_THICKNESS   = 0.5f;
  uint   const SLAB_INSTANCES   = 600;
  ushort const LIQUID_WIDTH     = 10;
  ushort const LIQUID_HEIGHT    = 10;
  ushort const LIQUID_TWIDTH    = 80;
  ushort const LIQUID_THEIGHT   = 80;
  ushort const LIQUID_RES       = LIQUID_TWIDTH * LIQUID_THEIGHT;
  char   const *FADE_FS_SRC     =
"#version 330 core\n"

"out vec4 frag;"

"uniform vec4 res;"
"uniform sampler2D srctex;"
"uniform float t;"

"void main(){"
  "float fade=clamp(1.-pow(max(0.,mod(t,5.2)-4.),4.),0.,1.);"
  "frag=texelFetch(srctex,ivec2(gl_FragCoord.xy),0)*fade;"
  "frag.w=1.;"
"}";
}

CubeRoom::CubeRoom(ushort width, ushort height, Common &common, Freefly const &freefly) :
    /* common */
    _width(width)
  , _height(height)
  , _fbCopier(width, height)
  , _freefly(freefly)
  , _drenderer(common.drenderer)
  , _matmgr(common.matmgr)
  , _stringRenderer(common.stringRenderer)
  , _fadePP("cube room fade", FADE_FS_SRC, width, height)
  , _slab(width, height, SLAB_SIZE, SLAB_THICKNESS)
  , _liquid(LIQUID_WIDTH, LIQUID_HEIGHT, LIQUID_TWIDTH, LIQUID_THEIGHT)
  , _laser(width, height, LASER_TESS_LEVEL, LASER_HHEIGHT) {
  _init_materials(width, height);
  _init_offscreen(width, height);
}

void CubeRoom::_init_materials(ushort width, ushort height) {
  _matmgrProjIndex   = _matmgr.postprocess().program().map_uniform("proj");
  _matmgrViewIndex   = _matmgr.postprocess().program().map_uniform("view");
  _matmgrLColorIndex = _matmgr.postprocess().program().map_uniform("lightColor");
  _matmgrLPosIndex   = _matmgr.postprocess().program().map_uniform("lightPos");
}

void CubeRoom::_init_offscreen(ushort width, ushort height) {
  /* offscreen texture */
  gTH.bind(Texture::T_2D, _offTex);
  gTH.parameter(Texture::P_WRAP_S, Texture::PV_CLAMP);
  gTH.parameter(Texture::P_WRAP_T, Texture::PV_CLAMP);
  gTH.parameter(Texture::P_MIN_FILTER, Texture::PV_NEAREST);
  gTH.parameter(Texture::P_MAG_FILTER, Texture::PV_NEAREST);
  gTH.parameter(Texture::P_MAX_LEVEL, 0);
  gTH.image_2D(width, height, 0, Texture::F_RGB, Texture::IF_RGB32F, GLT_FLOAT, 0, nullptr);
  gTH.unbind();

  /* offscreen renderbuffer */
  gRBH.bind(Renderbuffer::RENDERBUFFER, _offRB);
  gRBH.store(width, height, Texture::IF_DEPTH_COMPONENT32F);
  gRBH.unbind();

  /* offscreen framebuffer */
  gFBH.bind(Framebuffer::DRAW, _offFB);
  gFBH.attach_renderbuffer(_offRB, Framebuffer::DEPTH_ATTACHMENT);
  gFBH.attach_2D_texture(_offTex, Framebuffer::color_attachment(0));
  gFBH.unbind();
}

void CubeRoom::_draw_texts(float t) const {
  state::enable(state::BLENDING);
  Framebuffer::blend_func(blending::ONE, blending::ONE);
  _stringRenderer.start_draw();

  if (t < 41.f) {
  } else if (t < 61.5f) {
    _stringRenderer.draw_string("OHAI EVOKE2013! I AM HAPPY TO PRESENT YOU MY SECOND RELEASE", 1.f-(t-41.f)*0.5f, 0.25f, 0.08f);
    _stringRenderer.draw_string("HEAT STATION", 1.f-(t-51.f)*0.5f, 0.15f, 0.1f);
  } else {
    _stringRenderer.draw_string("THIS LITTLE 64K INTRO WAS WRITTEN BY ME SKYPERS", 1.f-(t-61.5f)*0.5f, -0.35f, 0.08f);
    _stringRenderer.draw_string("AND ITS SOUNDTRACK WAS PROVIDED BY GASPODE", 1.f-(t-65.5f)*0.5f, -0.2f, 0.08f);
    _stringRenderer.draw_string("THANK YOU GASPODE!", 1.f-(t-70.5f)*0.5f, 0.1f, 0.08f);
  }

  _stringRenderer.end_draw();
  state::disable(state::BLENDING);
}

void CubeRoom::run(float time) {
  /* projection & view */
  auto proj = Mat44::perspective(FOVY, 1.f * _width / _height, ZNEAR, ZFAR);
  auto yaw = Orient(Axis3(0.f, 1.f, 0.f), PI_2).to_matrix();
  auto pitch = Orient(Axis3(1.f, 0.f, 0.f), -PI_2).to_matrix();
  bool useFade = true;
  Mat44 view;

  //misc::log << debug << "CubeRoom::run()" << endl;
  
  /* FIXME: WHOOO THAT'S DIRTY!! DO YOU THINK SO?! */
  if (time <= 5.2f) {
    view = Mat44::trslt(-Position(1.f, 0.f, 0.f)) * yaw;
  } else if (time <= 10.4f) {
    view = Mat44::trslt(-Position(0.f, 1.f, 0.f)) * pitch;
  } else if (time <= 15.6f) {
    view = Mat44::trslt(-Position(1.f, 1.f, 1.f)) * Orient(Axis3(0.f, 1.f, 0.f), PI_2 / 3.).to_matrix();// * Orient(Axis3(1.f, 0.f, 0.f), -PI_4).to_matrix();
  } else if (time <= 20.8f) {
    view = Mat44::trslt(-Position(0.f, 1.f, 1.f)) * Orient(Axis3(0.f, 1.f, 0.f), PI_2 / 3.).to_matrix() * Orient(Axis3(1.f, 0.f, 0.f), -PI_4).to_matrix();
  } else {
    view = Mat44::trslt(-Position(cosf(time), sinf(time), sinf(time))*1.5f) * Orient(Axis3(0.f, 1.f, 0.f), time * PI_2 / 3.).to_matrix() * Orient(Axis3(0.f, 0.f, 1.f), sinf(time)+time*0.5f).to_matrix();
    if (time <= 75.f)
      useFade = false;
  }

  _drenderer.start_geometry();
  state::enable(state::DEPTH_TEST);
  state::clear(state::COLOR_BUFFER | state::DEPTH_BUFFER);
  _slab.render(time, proj, view, SLAB_INSTANCES);
  _liquid.render(time, proj, view, LIQUID_RES);
  _drenderer.end_geometry();

  gFBH.bind(Framebuffer::DRAW, _offFB);
  state::disable(state::DEPTH_TEST);
  state::enable(state::BLENDING);
  state::clear(state::COLOR_BUFFER | state::DEPTH_BUFFER);

  _drenderer.start_shading();
  _matmgr.start();

  _matmgrProjIndex.push(proj);
  _matmgrViewIndex.push(view);
  _matmgrLColorIndex.push(0.75f, 0.f, 0.f);
  _matmgrLPosIndex.push(0.f, 0.f, 0.f);
  _matmgr.render();

  _matmgr.end();
  _drenderer.end_shading();

  state::clear(state::DEPTH_BUFFER);

  _laser.render(time, proj, view, LASER_TESS_LEVEL);

  _draw_texts(time);
  gFBH.unbind();
  gFBH.unbind(); /* hihi... :DDDD */

  if (useFade) {
    gFBH.unbind();
    _fadePP.start();
    gTH.unit(0);
    gTH.bind(Texture::T_2D, _offTex);
    if (time <= 75.f)
      _fadePP.apply(time);
    else
      _fadePP.apply(time-75.f);
    gTH.unbind();
    _fadePP.end();
  } else {
    _fbCopier.copy(_offTex);
  }
}

