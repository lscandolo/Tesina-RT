#include <rt/renderer-config.hpp>
#include <rt/renderer.hpp>

RendererConfig::RendererConfig()
  : target(0)
{
}


void RendererConfig::set_target(Renderer* r)
{
        target = r;
}
