#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "particle_system.h"

void init_window(SDL_Window** window, SDL_Renderer** renderer) {
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    fprintf(stderr, "SDL everything init failed");
    return; 
  }

  if (IMG_Init(IMG_INIT_PNG) < 0) {
    fprintf(stderr, "SDL png init failed");
    return; 
  }

  *window = SDL_CreateWindow("sparkly", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600, SDL_WINDOW_SHOWN);
  
  if (*window == NULL) {
    fprintf(stderr, "window init failed");
    return; 
  }

  *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_SOFTWARE);
  
  if (*renderer == NULL) {
    fprintf(stderr, "renderer init failed");
    return; 
  }
}

SDL_Texture* load_png_into_texture(SDL_Renderer* renderer, const char* path, bool rescale, int rescale_x, int rescale_y) {
  SDL_Surface* image = IMG_Load(path);

  if (!rescale) {
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);
    return texture;
  }

  SDL_Surface* rescaled_image = SDL_CreateRGBSurface(
    0, 
    rescale_x,
    rescale_y,
    image->format->BitsPerPixel,
    image->format->Rmask,
    image->format->Gmask,
    image->format->Bmask,
    image->format->Amask
  );
  
  SDL_BlitScaled(image, NULL, rescaled_image, NULL);

  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, rescaled_image);

  SDL_FreeSurface(image);
  SDL_FreeSurface(rescaled_image);

  return texture;
}

void init_fireworks_base_config(EmitterConfig* ec) {
  ec->max_lifetime = 1;

  ec->spawn_frequency = 1;
  
  ec->start_angle_rad = 0 - PI / 2;
  ec->end_angle_rad = PI / 2;

  ec->particle_size = 6;
  
  ec->particle_speed = 280;

  ec->gravity_force = 0.581;

  ColorStop a = {1, 255, 255, 255, 255};
  ColorStop b = {1, 255, 255, 225, 255};
  ColorStop c = {0, 255, 255, 225, 0};

  ec->color_stops[0] = a;
  ec->color_stops[1] = b;
  ec->color_stops[2] = c;
  
  ec->color_stop_count = 3;

  ec->type = ET_CONTINOUS;
  ec->amount = EA_ALL;  
}

void init_fireworks_recurse_config(EmitterConfig* ec) {
  ec->max_lifetime = 1;

  ec->spawn_frequency = 40;
  
  ec->start_angle_rad = 0;
  ec->end_angle_rad = PI * 2;

  ec->particle_size = 4;
  
  ec->particle_speed = 180;

  ec->gravity_force = 0;

  ColorStop a = {1, 255, 255, 255, 255};
  ColorStop b = {1, 255, 180, 0, 125};
  ColorStop c = {0, 255, 0, 0, 0};

  ec->color_stops[0] = a;
  ec->color_stops[1] = b;
  ec->color_stops[2] = c;

  ec->type = ET_BURST;
  ec->amount = EA_SINGLE;

  ec->burst_interval = 0.49f;
  
  ec->color_stop_count = 3;
  ec->sub_emmissions_count = 0;
  ec->sub_emmissions = NULL;
}

void init_fireworks_light_config(EmitterConfig* ec) {
  ec->max_lifetime = 1;

  ec->spawn_frequency = 1.5;
  
  ec->start_angle_rad = 0;
  ec->end_angle_rad = 0;

  ec->particle_size = 2000;
  
  ec->particle_speed = 0;

  ec->gravity_force = 0;

  ColorStop a = {1, 125, 125, 125, 255};
  ColorStop b = {0, 125, 125, 125, 0};

  ec->color_stops[0] = a;
  ec->color_stops[1] = b;
  ec->color_stop_count = 2;

  ec->type = ET_BURST;
  ec->amount = EA_SINGLE;

  ec->burst_interval = 0.90f;
  
  ec->sub_emmissions_count = 0;
  ec->sub_emmissions = NULL;
}

int main() {
  SDL_Window* window;
  SDL_Renderer* renderer;

  init_window(&window, &renderer);

  bool running = true;

  SDL_Texture* texture = load_png_into_texture(renderer, "./sprites/circle.png", true, 16, 16);

  EmitterConfig emitter_config;
  EmitterConfig emitter_config_child;
  EmitterConfig emitter_config_child_2;
  init_fireworks_base_config(&emitter_config);
  init_fireworks_recurse_config(&emitter_config_child);
  init_fireworks_light_config(&emitter_config_child_2);

  EmitterConfig children[2];
  children[1] = emitter_config_child;
  children[0] = emitter_config_child_2;

  emitter_config.sub_emmissions = children;
  emitter_config.sub_emmissions_count = 2;

  ParticleSystem particle_system;
  init_particle_system(&particle_system, &emitter_config);

  print_emitter_config(particle_system.config);
  
  srand(time(NULL));

  double LAST = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
  double NOW;

  while (running) {
    NOW = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
    double delta_time = NOW - LAST;
    LAST = NOW;

    SDL_Event event;
    SDL_PollEvent(&event);

    switch (event.type) {
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        }
        break; 
      case SDL_QUIT:
        running = false;
        break;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
   
    update_particle_system(&particle_system, renderer, texture, delta_time);

    SDL_RenderPresent(renderer);
  }

  return 0;
}
