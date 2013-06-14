#include <core/state.hpp>
#include <fsm/cube_room.hpp>
#include <math/common.hpp>
#include <math/matrix.hpp>
#include <math/quaternion.hpp>
#include <misc/from_file.hpp>

using namespace sky;

namespace {
  float  const FOVY             = math::PI*70.f/180.f; /* 90 degrees */
  float  const ZNEAR            = 0.001f;
  float  const ZFAR             = 100.f;
  ushort const TESS_LASER_LEVEL = 13;
  ushort const BLUR_PASSES      = 3;
  float  const LASER_HHEIGHT    = 0.15;
}

/* ============
 * [ CubeRoom ]
 * ============ */
CubeRoom::CubeRoom(ushort width, ushort height) :
    /* common */
    _width(width)
  , _height(height)
  , _fbCopier(width, height)
    /* laser blur */
  , _laserHBlur("laser hblur", misc::from_file("../../src/fsm/laser_hblur-fs.glsl").c_str(), width, height)
  , _laserVBlur("laser vblur", misc::from_file("../../src/fsm/laser_vblur-fs.glsl").c_str(), width, height)
  , _laserMove("laser move", misc::from_file("../../src/fsm/laser_move-fs.glsl").c_str(), width, height) {
  /* laser */
  _init_laser_program(width, height);
  _laser.bind();
  _laser.unbind();
  _init_laser_texture(width, height);
  /* room */
  _init_room();
  _init_room_program(width, height);
}

CubeRoom::~CubeRoom() {
}

/* =========
 * [ Laser ]
 * ========= */
void CubeRoom::_init_laser_program(ushort width, ushort height) {
  core::Shader vs(core::Shader::VERTEX);
  core::Shader gs(core::Shader::GEOMETRY);
  core::Shader fs(core::Shader::FRAGMENT);

  /* sources compilation */
  vs.source(misc::from_file("../../src/fsm/laser-vs.glsl").c_str());
  vs.compile("laser VS");
  gs.source(misc::from_file("../../src/fsm/laser-gs.glsl").c_str());
  gs.compile("laser GS");
  fs.source(misc::from_file("../../src/fsm/laser-fs.glsl").c_str());
  fs.compile("laser FS");

  /* program link */
  _laserSP.attach(vs);
  _laserSP.attach(gs);
  _laserSP.attach(fs);
  _laserSP.link();

  _init_laser_uniforms(width, height);
  _init_laser_blur(width, height); 
}

void CubeRoom::_init_laser_uniforms(ushort width, ushort height) {
  auto vnbIndex     = _laserSP.map_uniform("vnb");
  auto hheightIndex = _laserSP.map_uniform("hheight");
  auto laserTex     = _laserSP.map_uniform("lasertex");
  _laserProjIndex   = _laserSP.map_uniform("proj");
  _laserViewIndex   = _laserSP.map_uniform("view");
  _laserTimeIndex   = _laserSP.map_uniform("t");

  _laserSP.use();

  vnbIndex.push(1.f * TESS_LASER_LEVEL, 1.f / TESS_LASER_LEVEL);
  hheightIndex.push(LASER_HHEIGHT);
  laserTex.push(0);

  _laserSP.unuse();
}

void CubeRoom::_init_laser_texture(ushort width, ushort height) {
  core::Framebuffer fb;
  core::FramebufferHandler fbh;
  core::Renderbuffer rb;
  core::RenderbufferHandler rbh;
  core::TextureHandler<1> texh;
  tech::PostProcess generator("laser texture generator", misc::from_file("../../src/fsm/laser_texgen-fs.glsl").c_str(), width, height);

  rbh.bind(core::Renderbuffer::RENDERBUFFER, rb);
  rbh.store(width, height, core::Texture::IF_DEPTH_COMPONENT);
  rbh.unbind();

  texh.bind(core::Texture::T_2D, _laserTexture);
  texh.parameter(core::Texture::P_WRAP_S, core::Texture::PV_CLAMP_TO_EDGE);
  texh.parameter(core::Texture::P_WRAP_T, core::Texture::PV_CLAMP_TO_EDGE);
  texh.parameter(core::Texture::P_MIN_FILTER, core::Texture::PV_LINEAR);
  texh.parameter(core::Texture::P_MAG_FILTER, core::Texture::PV_LINEAR);
  texh.image_2D(width, height, 0, core::Texture::F_RGB, core::Texture::IF_RGB, core::GLT_FLOAT, 0, nullptr);
  texh.unbind();

  fbh.bind(core::Framebuffer::DRAW, fb);
  fbh.attach_2D_texture(_laserTexture, core::Framebuffer::COLOR_ATTACHMENT);
  fbh.attach_renderbuffer(rb, core::Framebuffer::DEPTH_ATTACHMENT);

  /* make the texture */
  generator.start();
  generator.apply(0.f);
  generator.end();

  fbh.unbind();
}

void CubeRoom::_init_laser_blur(ushort width, ushort height) {
  core::FramebufferHandler fbh;
  core::RenderbufferHandler rbh;
  core::TextureHandler<1> texh;

  rbh.bind(core::Renderbuffer::RENDERBUFFER, _laserBlurRB);
  rbh.store(width, height, core::Texture::IF_DEPTH_COMPONENT);
  rbh.unbind();

  for (int i = 0; i < 2; ++i) {
    texh.bind(core::Texture::T_2D, _laserBlurOfftex[i]);
    texh.parameter(core::Texture::P_WRAP_S, core::Texture::PV_CLAMP_TO_BORDER);
    texh.parameter(core::Texture::P_WRAP_T, core::Texture::PV_CLAMP_TO_BORDER);
    texh.parameter(core::Texture::P_MIN_FILTER, core::Texture::PV_LINEAR);
    texh.parameter(core::Texture::P_MAG_FILTER, core::Texture::PV_LINEAR);
    texh.image_2D(width, height, 0, core::Texture::F_RGB, core::Texture::IF_RGB, core::GLT_FLOAT, 0, nullptr);
    texh.unbind();

    fbh.bind(core::Framebuffer::DRAW, _laserBlurFB[i]);
    fbh.attach_renderbuffer(_laserBlurRB, core::Framebuffer::DEPTH_ATTACHMENT);
    fbh.attach_2D_texture(_laserBlurOfftex[i], core::Framebuffer::COLOR_ATTACHMENT);
  }
}

void CubeRoom::_render_laser(float time, math::Mat44 const &proj, math::Mat44 const &view) const {
  static core::FramebufferHandler fbh;
  static core::TextureHandler<1> texh;
  int offtexid = 0;
 
  /* first, render the lined laser into a framebuffer */
  _laserSP.use();

  _laserProjIndex.push(proj);
  _laserViewIndex.push(view);

  _laserTimeIndex.push(time);
  fbh.bind(core::Framebuffer::DRAW, _laserBlurFB[0]);
  texh.bind(core::Texture::T_2D, _laserTexture);
  core::state::clear(core::state::COLOR_BUFFER | core::state::DEPTH_BUFFER);
  core::state::disable(core::state::DEPTH_TEST);
  _laser.render(core::primitive::LINE_STRIP, 0, TESS_LASER_LEVEL+1);
  texh.unbind();
  fbh.unbind();
  _laserSP.unuse();

  /* then, blur the lined laser */
  for (int i = 0; i < BLUR_PASSES; ++i) {
    /* first hblur */
    texh.bind(core::Texture::T_2D, _laserBlurOfftex[0]);
    fbh.bind(core::Framebuffer::DRAW, _laserBlurFB[1]);
    core::state::clear(core::state::COLOR_BUFFER | core::state::DEPTH_BUFFER);
    _laserHBlur.start();
    _laserHBlur.apply(0.);
    _laserHBlur.end();
    /* then vblur */
    texh.bind(core::Texture::T_2D, _laserBlurOfftex[1]);
    fbh.bind(core::Framebuffer::DRAW, _laserBlurFB[0]);
    core::state::clear(core::state::COLOR_BUFFER | core::state::DEPTH_BUFFER);
    _laserVBlur.start();
    _laserVBlur.apply(0.);
    _laserVBlur.end();
  }
  fbh.unbind();
  texh.unbind();

  /* add a moving effect on the blurred area
   * hint: the final blurred framebuffer id is 0 */
#if 0
  fbh.bind(core::Framebuffer::DRAW, _laserBlurFB[1]);
  texh.bind(core::Texture::T_2D, _laserBlurOfftex[0]);
  _laserMove.start();
  _laserMove.apply(time);
  _laserMove.end();
  fbh.unbind();
#endif

  /* then render the extremity with billboards */
  
  /* combine the blurred lined moving laser and billboards */
  _fbCopier.copy(_laserBlurOfftex[0]);
}

/* ========
 * [ Room ]
 * ======== */
void CubeRoom::_init_room() {
  core::BufferHandler bufh;

  uint const ids[] = {
    /* front face */
      0, 1, 2
    , 0, 2, 3
    /* back face */
    , 4, 5, 6
    , 4, 6, 7
    /* up face */
    , 0, 1, 4 
    , 1, 4, 5
    /* bottom face */
    , 2, 3, 7
    , 2, 7, 6
    /* left face */
    , 1, 2, 5
    , 2, 5, 6
    /* right face */
    , 0, 3, 4
    , 3, 4, 7
  };

  /* IBO */
  bufh.bind(core::Buffer::ELEMENT_ARRAY, _slabIBO);
  bufh.data(sizeof(uint)*36, core::Buffer::STATIC_DRAW, ids);
  bufh.unbind();

  /* VA */
  _slab.bind();
  bufh.bind(core::Buffer::ELEMENT_ARRAY, _slabIBO);
  _slab.unbind(); /* attribute-less render */
  bufh.unbind();
}

void CubeRoom::_init_room_program(ushort width, ushort height) {
  core::Shader vs(core::Shader::VERTEX);
  core::Shader gs(core::Shader::GEOMETRY);
  core::Shader fs(core::Shader::FRAGMENT);

  vs.source(misc::from_file("../../src/fsm/room-vs.glsl").c_str());
  vs.compile("room vertex shader");
  gs.source(misc::from_file("../../src/fsm/room-gs.glsl").c_str());
  gs.compile("room geometry shader");
  fs.source(misc::from_file("../../src/fsm/room-fs.glsl").c_str());
  fs.compile("room fragment shader");

  _slabSP.attach(vs);
  _slabSP.attach(gs);
  _slabSP.attach(fs);
  _slabSP.link();

  _init_room_uniforms(width, height);
}

void CubeRoom::_init_room_uniforms(ushort width, ushort height) {
  _slabProjIndex      = _slabSP.map_uniform("proj");
  _slabViewIndex      = _slabSP.map_uniform("view");
  _slabSizeIndex      = _slabSP.map_uniform("size");
  _slabThicknessIndex = _slabSP.map_uniform("thickness");
}

void CubeRoom::_render_room(float time, math::Mat44 const &proj, math::Mat44 const &view) const {
  _slabSP.use();
  
  /* move camera around */
  _slabProjIndex.push(proj);
  _slabViewIndex.push(view);

  /* push size and thickness */
  _slabSizeIndex.push(1.f);
  _slabThicknessIndex.push(1.f);
  
  /* render walls */
  _slab.bind();
  _slab.inst_indexed_render(core::primitive::TRIANGLE, 36, core::GLT_UINT, 150);
  _slab.unbind();
  
  _slabSP.unuse();
}

/* ============
 * [ CubeRoom ]
 * ============ */
void CubeRoom::run(float time) const {
  static core::FramebufferHandler fbh;

  /* projection */
  auto proj = math::Mat44::perspective(FOVY, 1.f * _width / _height, ZNEAR, ZFAR);
  auto view = math::Mat44::trslt(-math::Vec3<float>(1.f, cosf(time), sinf(time))*3.f) * math::Quat(math::Axis3(0.f, 1.f, 0.f), math::PI_2+time*0.5f).to_matrix();

  /* view */
  fbh.unbind(); /* back to the default framebuffer */

  core::state::enable(core::state::DEPTH_TEST);
  core::state::clear(core::state::COLOR_BUFFER | core::state::DEPTH_BUFFER);

  _render_room(time, proj, view);
  core::state::enable(core::state::BLENDING);
  core::Framebuffer::blend_func(core::blending::ONE, core::blending::ONE);
  _render_laser(time, proj, view);
  core::state::disable(core::state::BLENDING);
}

