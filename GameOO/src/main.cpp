#include "game.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
#include <SDL.h>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <ngl/NGLInit.h>
#include <ngl/SimpleVAO.h>
#include <ngl/ShaderLib.h>
#include <ngl/Util.h>
#include <ngl/VAOFactory.h>
#include <ngl/Vec3.h>
#include <ngl/Vec4.h>
#include <ngl/NGLStream.h>
#include <ngl/Random.h>
#include "Benchmark.h"
/// @brief function to quit SDL with error message
/// @param[in] _msg the error message to send
[[noreturn]] void SDLErrorExit(const std::string &_msg);

/// @brief initialize SDL OpenGL context
SDL_GLContext createOpenGLContext( SDL_Window *window);

const int kObjectCount = 1024*10;
const int kAvoidCount = 16*1;




int main(int argc, char * argv[])
{
  // under windows we must use main with argc / v so jus flag unused for params
  NGL_UNUSED(argc);
  NGL_UNUSED(argv);
  initialize(kObjectCount,kAvoidCount);
  std::unique_ptr<RenderData []> sprite_data = std::make_unique<RenderData []>(kMaxSpriteCount);

  // Initialize SDL's Video subsystem
  if (SDL_Init(SDL_INIT_VIDEO) < 0 )
  {
    // Or die on error
    SDLErrorExit("Unable to initialize SDL");
  }

  // now get the size of the display and create a window we need to init the video
  SDL_Rect rect;
  SDL_GetDisplayBounds(0,&rect);
  // now create our window
  std::string title=fmt::format("Game Using OO {0} Objects {1} Avoid Objects",kObjectCount,kAvoidCount);

  SDL_Window *window=SDL_CreateWindow(title.c_str(),
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED,
                                      rect.w/2,
                                      rect.h/2,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
                                     );
  // check to see if that worked or exit
  if (!window)
  {
    SDLErrorExit("Unable to create window");
  }

  // Create our opengl context and attach it to our window

   SDL_GLContext glContext=createOpenGLContext(window);
   if(!glContext)
   {
     SDLErrorExit("Problem creating OpenGL context ");
   }
   // make this our current GL context (we can have more than one window but in this case not)
   SDL_GL_MakeCurrent(window, glContext);
  /* This makes our buffer swap syncronized with the monitor's vertical refresh */
  SDL_GL_SetSwapInterval(1);
  // we need to initialise the NGL lib which will load all of the OpenGL functions, this must
  // be done once we have a valid GL context but before we call any GL commands. If we dont do
  // this everything will crash
  // now clear the screen and swap whilst NGL inits (which may take time)
  ngl::NGLInit::instance();
  auto shader = ngl::ShaderLib::instance();
  shader->loadShader("PointShader","shaders/PointVertex.glsl","shaders/PointFragment.glsl");
  ngl::Mat4 projection=ngl::ortho(-180.0f,180.0f,-150.0f,150.0f);
  std::cout<<projection<<'\n';
  shader->use("PointShader");
  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
  glEnable(GL_MULTISAMPLE);


  shader->setUniform("projection",projection);
  glClearColor(0.6f,0.6f,0.6f,1.0);
  //glPointSize(5.0);
  glViewport(0,0,rect.w/2,rect.h/2);
  ngl::Random::instance()->setSeed();
  auto vao=ngl::VAOFactory::createVAO(ngl::simpleVAO,GL_POINTS);


  glClear(GL_COLOR_BUFFER_BIT);
  SDL_GL_SwapWindow(window);
  // flag to indicate if we need to exit
  bool quit=false;
  // sdl event processing data structure
  SDL_Event event;
  Benchmark <>updateBenchmark(2000);
  Benchmark <>renderBenchmark(2000);
  auto end= std::chrono::system_clock::now();
  auto start = std::chrono::system_clock::now();

  while(!quit)
  {
    glClear(GL_COLOR_BUFFER_BIT);

    while ( SDL_PollEvent(&event) )
    {
      switch (event.type)
      {
        // this is the window x being clicked.
        case SDL_QUIT : quit = true; break;

        // now we look for a keydown event
        case SDL_KEYDOWN:
        {
          switch( event.key.keysym.sym )
          {
            // if it's the escape key quit
            case SDLK_ESCAPE :  quit = true; break;
            default : break;
          } // end of key process
        } // end of keydown

      } // end of event switch
    } // end of poll events
    start = std::chrono::system_clock::now();
    size_t sprite_count;
    std::chrono::duration<float> elapsed_seconds = end-start;
    {
      auto updatebegin = std::chrono::steady_clock::now();
      sprite_count = update(sprite_data.get(), elapsed_seconds.count());
      auto updateend = std::chrono::steady_clock::now();

      ngl::msg->addMessage(fmt::format("Update took {0} uS",std::chrono::duration_cast<std::chrono::microseconds> (updateend - updatebegin).count()));
      updateBenchmark.addDuration(updateend - updatebegin);

    }

    {
    auto renderbegin = std::chrono::steady_clock::now();
    vao->bind();
    vao->setData( ngl::SimpleVAO::VertexData(sizeof(RenderData)*(kObjectCount+kAvoidCount),sprite_data[0].pos.m_x));
    // We must do this each time as we change the data.
    vao->setVertexAttributePointer(0,2,GL_FLOAT,sizeof(RenderData),0);
    vao->setVertexAttributePointer(1,4,GL_FLOAT,sizeof(RenderData),2);
    vao->setNumIndices(kObjectCount+kAvoidCount);
    vao->draw();
    vao->unbind();
    auto renderend = std::chrono::steady_clock::now();
    ngl::msg->addMessage(fmt::format("Render took {0} uS",std::chrono::duration_cast<std::chrono::microseconds> (renderend - renderbegin).count()));
    renderBenchmark.addDuration(renderend - renderbegin);

    }

    end = std::chrono::system_clock::now();
    // swap the buffers
    SDL_GL_SwapWindow(window);

  }
  std::cout<<"Render Benchmarks Min "<<renderBenchmark.min()
           <<" uS Max "<<renderBenchmark.max()
           <<" uS Average "<<renderBenchmark.average()
           <<" uS Median "<<renderBenchmark.median()<<" uS\n";

  std::cout<<"Update Benchmarks Min "<<updateBenchmark.min()
           <<" uS Max "<<updateBenchmark.max()
           <<" uS Average "<<updateBenchmark.average()
           <<" uS Median "<<updateBenchmark .median()<<" uS\n";

  updateBenchmark.mode();
  renderBenchmark.mode();

  // now tidy up and exit SDL
 SDL_Quit();
 teardown();
 // whilst this code will never execute under windows we need to have a return from
 // SDL_Main!
 return EXIT_SUCCESS;
}


SDL_GLContext createOpenGLContext(SDL_Window *window)
{
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  // set multi sampling else we get really bad graphics that alias
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);
  return SDL_GL_CreateContext(window);

}

[[noreturn]] void SDLErrorExit(const std::string &_msg)
{
  std::cerr<<_msg<<"\n";
  std::cerr<<SDL_GetError()<<"\n";
  SDL_Quit();
  exit(EXIT_FAILURE);
}



