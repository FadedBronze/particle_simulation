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
  EC_LIFE_START,
} EMIT_CONDITION;

typedef enum EMIT_TYPE {
  ET_BURST,
  ET_CONTINOUS,
  ET_NONE,
} EMIT_TYPE;

typedef struct EmitterConfig { 
  float start_angle_rad;
  float end_angle_rad;

  float max_lifetime;
  //particles per second
  float spawn_frequency;

  float burst_interval;
  
  int particle_size;
  
  float particle_speed;
  float gravity_force;
  
  ColorStop color_stops[16];
  int color_stop_count;
 
  EMIT_CONDITION condition;
  EMIT_TYPE type;

  int sub_emmissions_count;
  struct EmitterConfig* sub_emmissions;
} EmitterConfig;

typedef struct Particle {
  float x;
  float y;
  float x_velocity;
  float y_velocity;
  float travel_time_secs;
  
  unsigned long emit_count;
  EmitterConfig* emitting_config;
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

void update_particles(ParticleSystem* ps, double delta_time) {
  for (int i = 0; i < ps->particle_count; i++) {
    Particle* particle = &ps->particles[i];

    particle->travel_time_secs += delta_time;
    
    if (particle->config == NULL) continue;

    particle->y_velocity += particle->config->gravity_force * delta_time;

    particle->x += particle->x_velocity * delta_time * particle->config->particle_speed;
    particle->y += particle->y_velocity * delta_time * ps->config->particle_speed;

    if (particle->travel_time_secs > particle->config->max_lifetime) {
      Particle temp = ps->particles[ps->particle_count - 1];
      ps->particles[ps->particle_count - 1] = *particle;
      *particle = temp;

      ps->particle_count -= 1;
      i -= 1;
    }
  }
}

void spawn_particles(ParticleSystem* ps, double delta_time) {
  for (int i = 0; i < ps->particle_count; i++) {
    Particle* particle = &ps->particles[i]; 

    if (particle->emitting_config == NULL) continue;
    if (particle->travel_time_secs == 0) continue;
    
    EmitterConfig* particle_emitting_config = particle->emitting_config;

    double range = particle_emitting_config->start_angle_rad - particle_emitting_config->end_angle_rad; 
    
    double particles_per_second = particle->emit_count / particle->travel_time_secs;
 
    double target_spawn_frequency = particle->emitting_config->spawn_frequency;

    if (
        particle_emitting_config->type == ET_BURST && 
        particles_per_second + particle_emitting_config->burst_interval > target_spawn_frequency
        ) {
      continue;
    }

    while (particles_per_second < target_spawn_frequency) {
      double angle = rand01() * range + particle->emitting_config->start_angle_rad;
      int type = rand01() * particle->emitting_config->sub_emmissions_count;

      Particle new_particle;

      new_particle.travel_time_secs = 0;
      new_particle.y_velocity = cos(angle);
      new_particle.x_velocity = sin(angle);
      new_particle.x = particle->x;
      new_particle.y = particle->y;
      new_particle.emit_count = 0;
      new_particle.config = particle->emitting_config;
      new_particle.emitting_config = &particle->emitting_config->sub_emmissions[type];

      ps->particles[ps->particle_count] = new_particle; 

      ps->particle_count += 1;
      particle->emit_count += 1;
      
      particles_per_second = particle->emit_count / particle->travel_time_secs;
    }
  }
}

void render_particles(ParticleSystem* ps, SDL_Renderer* renderer, SDL_Texture* particle_image) {
  for (int i = 0; i < ps->particle_count; i++) {
    Particle* particle = &ps->particles[i];
    
    if (particle->config == NULL) continue;

    EmitterConfig* particle_config = particle->config;

    const double half_particle_size = particle_config->particle_size / 2.0f;

    int total = 0;

    for (int i = 0; i < particle_config->color_stop_count - 1; i++) {
      total += ps->config->color_stops[i].ratio;
    }

    SDL_Rect render_rect;
    render_rect.x = particle->x - half_particle_size;
    render_rect.y = particle->y - half_particle_size;
    render_rect.w = half_particle_size * 2.0f;
    render_rect.h = half_particle_size * 2.0f;

    const double age = particle->travel_time_secs / particle->config->max_lifetime;
    double min_age = 0;
    
    for (int j = 0; j < particle_config->color_stop_count - 1; j++) {
      const double current_age_addition = particle_config->color_stops[j].ratio / (double)total;
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

      int next = j+1;
      
      Uint8 red = particle_config->color_stops[next].r * min_ratio + particle_config->color_stops[j].r * max_ratio;
      Uint8 green = particle_config->color_stops[next].g * min_ratio + particle_config->color_stops[j].g * max_ratio;
      Uint8 blue = particle_config->color_stops[next].b * min_ratio + particle_config->color_stops[j].b * max_ratio;

      SDL_SetTextureColorMod(particle_image, red, green, blue);

      Uint8 alpha = particle_config->color_stops[next].a * min_ratio + particle_config->color_stops[j].a * max_ratio;

      SDL_SetTextureAlphaMod(particle_image, alpha); 

      min_age = min_age + current_age_addition;
    }

    SDL_RenderCopy(renderer, particle_image, NULL, &render_rect);  
    
    SDL_SetTextureColorMod(particle_image, 255, 255, 255);

    SDL_SetTextureAlphaMod(particle_image, 255); 
  }
}

void update_particle_system(ParticleSystem* ps, SDL_Renderer* renderer, SDL_Texture* particle_image, double delta_time) {
  spawn_particles(ps, delta_time);
  update_particles(ps, delta_time);
  render_particles(ps, renderer, particle_image);
}

void print_color_stop(const ColorStop* color_stop) {
  printf(
    "r: %i, g: %i b: %i, a: %i\n", 
    color_stop->r, 
    color_stop->g, 
    color_stop->b, 
    color_stop->a
  );
} 

void print_emit_condition(EMIT_CONDITION condition) {
  switch (condition) {
    case EC_LIFE_START:
      printf("EC_LIFE_START\n");
    break;
    case EC_LIFE_END:
      printf("EC_LIFE_END\n");
    break;
    default:
      printf("Unhandled Case\n");
    break;
  }
}

void print_emit_type(EMIT_TYPE type) {
  switch (type) {
    case ET_NONE:
      printf("ET_NONE\n");
    break;
    case ET_CONTINOUS:
      printf("ET_CONTINOUS\n");
    break;
    case ET_BURST:
      printf("ET_BURST\n");
    break;
    default:
      printf("Unhandled Case\n");
    break;
  }
}

void print_emitter_config(const EmitterConfig* ec) {
  printf("max_lifetime %f\n", ec->max_lifetime);
  printf("spawn_frequency %f\n", ec->spawn_frequency);
  printf("start_angle_rad %f\n", ec->start_angle_rad);
  printf("end_angle_rad %f\n", ec->end_angle_rad);
  printf("particle_size %i\n", ec->particle_size);
  printf("particle_speed %f\n", ec->particle_speed);
  printf("gravity_force %f\n", ec->gravity_force);

  printf("color_stop_count %i\n", ec->color_stop_count);

  for (int i = 0; i < ec->color_stop_count; i++) {
    printf("index: %i | ", i); 
    print_color_stop(&ec->color_stops[i]);
  }

  print_emit_type(ec->type);
  print_emit_condition(ec->condition);

  printf("child_count %i\n", ec->sub_emmissions_count);
  
  for (int i = 0; i < ec->sub_emmissions_count; i++) {
    printf("\nchild %i\n\n", i);
    print_emitter_config(&ec->sub_emmissions[i]);
    printf("\n");
  }
}

void init_fireworks_base_config(EmitterConfig* ec) {
  //settings basically
  ec->max_lifetime = 2;

  ec->spawn_frequency = 0.5;
  
  ec->start_angle_rad = 0 - PI / 2;
  ec->end_angle_rad = PI / 2;

  ec->particle_size = 6;
  
  ec->particle_speed = 120;

  ec->gravity_force = 0.0981;

  ColorStop a = {1, 255, 255, 255, 255};
  ColorStop b = {1, 0, 180, 225, 125};
  ColorStop c = {1, 0, 0, 255, 0};

  ec->color_stops[0] = a;
  ec->color_stops[1] = b;
  ec->color_stops[2] = c;

  ec->condition = EC_LIFE_END;
  ec->type = ET_CONTINOUS;
  
  ec->color_stop_count = 3;
  ec->sub_emmissions_count = 0;
  ec->sub_emmissions = NULL;
}


void init_fireworks_recurse_config(EmitterConfig* ec) {
  //settings basically
  ec->max_lifetime = 1;

  ec->spawn_frequency = 50;
  
  ec->start_angle_rad = 0;
  ec->end_angle_rad = PI * 2;

  ec->particle_size = 4;
  
  ec->particle_speed = 100;

  ec->gravity_force = 0.0981;

  ColorStop a = {1, 255, 255, 255, 255};
  ColorStop b = {1, 0, 180, 225, 125};
  ColorStop c = {1, 0, 0, 255, 0};

  ec->color_stops[0] = a;
  ec->color_stops[1] = b;
  ec->color_stops[2] = c;

  ec->condition = EC_LIFE_END;
  ec->type = ET_BURST;

  ec->burst_interval = 1.9f;
  
  ec->color_stop_count = 3;
  ec->sub_emmissions_count = 0;
  ec->sub_emmissions = NULL;
}

void init_particle_system(ParticleSystem* ps, EmitterConfig* ec) {
  ps->config = ec;
  ps->particle_count = 1;
  ps->origin_x = 600 / 2.0f; 
  ps->origin_y = 400 / 2.0f; 

  Particle root_emitter;
  root_emitter.x = ps->origin_x;
  root_emitter.y = ps->origin_y;
  root_emitter.x_velocity = 0;
  root_emitter.y_velocity = 0;
  root_emitter.config = NULL;
  root_emitter.emitting_config = ps->config;
  root_emitter.emit_count = 0;
  root_emitter.travel_time_secs = 0;

  ps->particles[0] = root_emitter;
}

int main() {
  SDL_Window* window;
  SDL_Renderer* renderer;

  init_window(&window, &renderer);

  bool running = true;

  SDL_Texture* texture = load_png_into_texture(renderer, "./sprites/circle.png", true, 16, 16);

  EmitterConfig emitter_config;
  EmitterConfig emitter_config_child;
  init_fireworks_base_config(&emitter_config);
  init_fireworks_recurse_config(&emitter_config_child);
  emitter_config.sub_emmissions = &emitter_config_child;
  emitter_config.sub_emmissions_count = 1;

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
