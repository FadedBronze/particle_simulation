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
#include <math.h>

void init_window(SDL_Window** window, SDL_Renderer** renderer) {
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    fprintf(stderr, "SDL everything init failed");
    return; 
  }

  if (IMG_Init(IMG_INIT_PNG) < 0) {
    fprintf(stderr, "SDL png init failed");
    return; 
  }

  *window = SDL_CreateWindow("sparkly", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 400, SDL_WINDOW_SHOWN);
  
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

typedef struct ColorStop {
   Uint8 ratio;
   Uint8 r;
   Uint8 g;
   Uint8 b;
   Uint8 a;
} ColorStop;

#define PI 3.14159265358979323846

#define MAX_PARTICLES 10000
#define MAX_EMITTERS 100

typedef enum EMIT_CONDITION {
  EC_LIFE_END,
  EC_CONTINUOUS,
  EC_SPAWN,
} EMIT_CONDITION;

typedef enum EMIT_TYPE {
  ET_BURST,
  ET_CONTINOUS,
  ET_NONE,
} EMIT_TYPE;

typedef struct EmitterConfig { 
  float origin_x;
  float origin_y;
  float start_angle_rad;
  float end_angle_rad;

  float max_lifetime;
  //particles per second
  float spawn_frequency;
  
  int particle_size;
  
  float particle_speed;
  float gravity_force;
  
  ColorStop color_stops[16];
  int color_stop_count;
 
  EMIT_CONDITION condition;
  EMIT_TYPE type;

  struct EmitterConfig* children;
} EmitterConfig;

typedef struct Particle {
  float x;
  float y;
  float x_velocity;
  float y_velocity;
  float travel_time_secs;
  
  unsigned long emit_running_count;
  EmitterConfig* config;
} Particle;

typedef struct ParticleSystem {
  Particle particles[MAX_PARTICLES];
  EmitterConfig* config;
  int particle_count;

  float origin_x;
  float origin_y;
} ParticleSystem;

double rand01() {
  return (double)rand() / RAND_MAX;
}

void update_particles(EmitterConfig* ps, double delta_time) {
  for (int i = 0; i < ps->_particle_count; i++) {
    Particle* particle = &ps->particles[i]; 

    particle->y_velocity += ps->gravity_force * delta_time;

    particle->x += particle->x_velocity * delta_time * ps->particle_speed;
    particle->y += particle->y_velocity * delta_time * ps->particle_speed;
    particle->travel_time_secs += delta_time;

    if (particle->travel_time_secs > ps->max_lifetime) {
      Particle temp = ps->particles[ps->_particle_count - 1];
      ps->particles[ps->_particle_count - 1] = *particle;
      *particle = temp;

      ps->_particle_count -= 1;
      i -= 1;
      //avoid skipping the swapped particle
    }
  }
}

void spawn_particles(EmitterConfig* ps, double delta_time) {
  double range = ps->start_angle_rad - ps->end_angle_rad; 

  ps->_running_time += delta_time;

  double particles_per_second = ps->_running_count / ps->_running_time;
  double target_spawn_frequency = ps->spawn_frequency;

  while (particles_per_second < target_spawn_frequency) {
    double angle = rand01() * range + ps->start_angle_rad;

    Particle particle;
    
    particle.travel_time_secs = 0;
    particle.y_velocity = cos(angle);
    particle.x_velocity = sin(angle);
    particle.x = ps->origin_x;
    particle.y = ps->origin_y;

    ps->particles[ps->_particle_count] = particle; 

    ps->_particle_count += 1;
    ps->_running_count += 1;
    particles_per_second = ps->_running_count / ps->_running_time;    
  }
}

void render_particles(EmitterConfig* ps, SDL_Renderer* renderer, SDL_Texture* particle_image) {
  const double half_particle_size = ps->particle_size / 2.0f;

  int total = 0;

  for (int i = 0; i < ps->color_stop_count; i++) {
    total += ps->color_stops[i].ratio;
  }

  for (int i = 0; i < ps->_particle_count; i++) {
    Particle* particle = &ps->particles[i];

    SDL_Rect render_rect;
    render_rect.x = particle->x - half_particle_size;
    render_rect.y = particle->y - half_particle_size;
    render_rect.w = ps->particle_size;
    render_rect.h = ps->particle_size;

    const double age = particle->travel_time_secs / ps->max_lifetime;
    double min_age = 0;
    
    for (int j = 0; j < ps->color_stop_count; j++) {
      const double current_age_addition = ps->color_stops[j].ratio / (double)total;
      const double max_age = current_age_addition + min_age;
 
      if (age < min_age || age > max_age) {
        min_age += current_age_addition;
        continue; 
      }

      const double min_delta = age - min_age;
      const double max_delta = max_age - age;

      const double scale_amount = 1 / current_age_addition;
    
      const double max_ratio = scale_amount * max_delta;
      const double min_ratio = scale_amount * min_delta;

      int next = (j+1) % ps->color_stop_count;
      
      Uint8 red = ps->color_stops[next].r * min_ratio + ps->color_stops[j].r * max_ratio;
      Uint8 green = ps->color_stops[next].g * min_ratio + ps->color_stops[j].g * max_ratio;
      Uint8 blue = ps->color_stops[next].b * min_ratio + ps->color_stops[j].b * max_ratio;

      //printf("%f %f\n", max_ratio, min_ratio);

      if (i == 0) {
        //printf("red: %i green: %i blue: %i\n", 
        //  ps->color_stops[j].r, 
        //  ps->color_stops[j].g, 
        //  ps->color_stops[j].b
        //);

        //printf("red: %i green: %i blue: %i\n", 
        //  ps->color_stops[next].r, 
        //  ps->color_stops[next].g, 
        //  ps->color_stops[next].b
        //);
        //

        printf("min age: %f, age: %f, max age: %f\n", min_age, age, max_age);

        printf("red: %i green: %i blue: %i\n", red, green, blue);
      
        printf("\n");
      }

      SDL_SetTextureColorMod(particle_image, red, green, blue);

      Uint8 alpha = ps->color_stops[next].a * min_ratio + ps->color_stops[j].a * max_ratio;

      SDL_SetTextureAlphaMod(particle_image, alpha); 

      min_age = min_age + current_age_addition;
    }

    SDL_RenderCopy(renderer, particle_image, NULL, &render_rect);  
    
    SDL_SetTextureColorMod(particle_image, 255, 255, 255);

    SDL_SetTextureAlphaMod(particle_image, 255); 
  }
}

void update_particle_system(EmitterConfig* ps, SDL_Renderer* renderer, SDL_Texture* particle_image, double delta_time) {
  spawn_particles(ps, delta_time);
  update_particles(ps, delta_time);
  render_particles(ps, renderer, particle_image);
}

void init_particle_system(EmitterConfig* ps) {
  //changes at runtime 
  ps->_particle_count = 0;
  ps->_running_count = 0;
  ps->_running_time = 0;

  //settings basically
  ps->origin_x = 600.0f / 2;
  ps->origin_y = 400.0f / 2;

  ps->max_lifetime = 20;

  ps->spawn_frequency = 15;
  
  ps->start_angle_rad = 0 - PI / 2;
  ps->end_angle_rad = PI / 2;

  ps->particle_size = 16;
  
  ps->particle_speed = 20;

  ps->gravity_force = 0.0981;

  ColorStop a = {1, 255, 255, 255, 255};
  ColorStop b = {1, 0, 180, 225, 125};
  ColorStop c = {1, 0, 0, 255, 0};

  ps->color_stops[0] = a;
  ps->color_stops[1] = b;
  ps->color_stops[2] = c;
  
  ps->color_stop_count = 4;
}

int main() {
  SDL_Window* window;
  SDL_Renderer* renderer;

  init_window(&window, &renderer);

  bool running = true;

  SDL_Texture* texture = load_png_into_texture(renderer, "./sprites/circle.png", true, 16, 16);

  EmitterConfig particle_system;
  init_particle_system(&particle_system);

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
