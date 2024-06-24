#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>

#define PI 3.14159265358979323846

#define MAX_PARTICLES 10000
#define MAX_EMITTERS 100

typedef struct ColorStop {
   Uint8 ratio;
   Uint8 r;
   Uint8 g;
   Uint8 b;
   Uint8 a;
} ColorStop;

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
  float last_burst_time;
  
  unsigned long emit_count;
  EmitterConfig* emitting_config;
  EmitterConfig* config;
} Particle;

typedef struct ParticleSystem {
  Particle particles[MAX_PARTICLES];
  EmitterConfig* config;
  int particle_count;
} ParticleSystem;

void init_particle_system(ParticleSystem* ps, EmitterConfig* ec);

void print_emitter_config(const EmitterConfig* ec);

void update_particle_system(ParticleSystem* ps, SDL_Renderer* renderer, SDL_Texture* particle_image, double delta_time);

#endif
