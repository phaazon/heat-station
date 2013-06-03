#ifndef __FSM_CUBE_ROOM_HPP
#define __FSM_CUBE_ROOM_HPP

#include <core/framebuffer.hpp>
#include <core/renderbuffer.hpp>
#include <core/shader.hpp>
#include <core/texture.hpp>
#include <core/vertex_array.hpp>
#include <lang/primtypes.hpp>
#include <sync/parts_fsm.hpp>
#include <tech/framebuffer_copy.hpp>
#include <tech/post_process.hpp>

class CubeRoom : public sky::sync::FinalPartState {
  /* common */
  sky::ushort _width, _height;
  sky::tech::DefaultFramebufferCopy _fbCopier;

  /* laser */
  sky::core::VertexArray _laser;
  sky::core::Program _laserSP;
  void _init_laser_program(sky::ushort width, sky::ushort height);
  sky::core::Program::Uniform _laserTimeIndex;
  void _init_laser_uniforms(sky::ushort width, sky::ushort height);

  /* laser blur */
  sky::tech::PostProcess _laserHBlur;
  sky::tech::PostProcess _laserVBlur;
  sky::core::Framebuffer _laserBlurFB[2];
  sky::core::Renderbuffer _laserBlurRB;
  sky::core::Texture _laserBlurOfftex[2];
  void _init_laser_blur(sky::ushort width, sky::ushort height);

  void _render_laser(float time) const;

public :
  CubeRoom(sky::ushort width, sky::ushort height);
  ~CubeRoom(void);

  void run(float time) const;
};

#endif /* guard */

